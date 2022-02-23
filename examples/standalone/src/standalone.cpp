
#include <metrisca.hpp>

using namespace metrisca;

class TestLoader : public LoaderPlugin {
public:
    virtual Result<void, Error> Init(const ArgumentList& args) override
    {
        auto filename = args.GetString("file");
        if(!filename.has_value())
            return Error::MISSING_ARGUMENT;

        m_Filename = filename.value();

        return {};
    }

    virtual Result<void, Error> Load(TraceDatasetBuilder& builder) override
    {
        // The file contains 256 traces of 5000 samples each
        unsigned int num_traces = 256;
        unsigned int num_samples = 5000;

        // Fill in the required fields in the builder
        builder.EncryptionType = EncryptionAlgorithm::S_BOX;
        builder.CurrentResolution = 1e-6;
        builder.TimeResolution = 1e-3;
        builder.PlaintextMode = PlaintextGenerationMode::RANDOM;
        builder.PlaintextSize = 1;
        builder.KeyMode = KeyGenerationMode::FIXED;
        builder.KeySize = 1;
        builder.NumberOfTraces = num_traces;
        builder.NumberOfSamples = num_samples;

        // Open the file
        std::ifstream file(m_Filename);
        if(!file)
        {
            // If the file does not exist, return the appropriate error code.
            return Error::FILE_NOT_FOUND;
        }

        std::string line;
        unsigned int line_count = 0;

        std::vector<int> trace;
        trace.resize(num_samples);

        while(std::getline(file, line))
        {
            // Parse a line in the file and extract the trace measurement
            std::string trace_val_str = line.substr(line.find(' ') + 1, line.size());
            double trace_val = stod(trace_val_str);

            // Convert the current measurement into an integer value and accumulate the trace
            uint32_t sample_num = line_count % num_samples;
            trace[sample_num] = (int)(trace_val / builder.CurrentResolution);

            // Once the end of the trace is reached, add to the builder
            if(sample_num == num_samples - 1)
            {
                builder.AddTrace(trace);
            }

            if(++line_count >= num_traces * num_samples)
                break;
        }

        // The plaintexts go from 0 to 255 in order
        for(unsigned int p = 0; p < num_traces; ++p)
        {
            builder.AddPlaintext({ (unsigned char)p });
        }

        // The key is fixed to 0
        builder.AddKey({ (unsigned char)0 });

        return {};
    }

private:
    std::string m_Filename;

};

int main(void)
{
    // Initialize the library's components and register the custom loader
    Logger::Init(LogLevel::Info);
    PluginFactory::Init();
    METRISCA_REGISTER_PLUGIN(TestLoader, "testloader");

    // Create the argument list
    // Note that it can be reused across plugins
    ArgumentList args;
    // We set the dataset file to Trace1.txt
    args.SetString("file", "Trace1.txt");

    // Instantiate the loader
    auto loader_or_error = PluginFactory::The().ConstructAs<LoaderPlugin>(PluginType::Loader, "testloader", args);
    if(loader_or_error.IsError())
    {
        METRISCA_ERROR("Loader creation failed with code {}!", loader_or_error.Error());
        return 1;
    }

    // Load the dataset from the file
    auto loader = loader_or_error.Value();
    TraceDatasetBuilder builder;
    loader->Load(builder);
    auto dataset_or_error = builder.Build();
    if(dataset_or_error.IsError())
    {
        METRISCA_ERROR("Dataset creation failed with code {}!", dataset_or_error.Error());
        return 1;
    }
    auto dataset = dataset_or_error.Value();

    // Set the remaining arguments needed to instantiate the metric
    args.SetDataset(ARG_NAME_DATASET, dataset);
    args.SetString(ARG_NAME_DISTINGUISHER, "pearson");
    args.SetString(ARG_NAME_MODEL, "hamming_distance");
    args.SetString(ARG_NAME_OUTPUT_FILE, "guess.csv");

    // Instantiate the required metric
    auto metric_or_error = PluginFactory::The().ConstructAs<MetricPlugin>(PluginType::Metric, "guess", args);
    if(metric_or_error.IsError())
    {
        METRISCA_ERROR("Metric creation failed with code {}!", metric_or_error.Error());
        return 1;
    }
    auto metric = metric_or_error.Value();

    // Compute the metric and save the output to guess.csv
    auto result = metric->Compute();

    return 0;
}