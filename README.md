# MetriSCA - A side-channel analysis library

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.5778947.svg)](https://doi.org/10.5281/zenodo.5778947)
![Build workflow](https://github.com/mirjanastojilovic/MetriSCA/actions/workflows/build.yml/badge.svg)
![Test workflow](https://github.com/mirjanastojilovic/MetriSCA/actions/workflows/test.yml/badge.svg)

> :warning: This is the latest development version of the project. Please refer to this repository's tags or go to 
> [Zenodo](https://zenodo.org/record/5778947)'s website for a list of published releases.

The MetriSCA library contains various tools for side-channel analysis. It focuses on bringing performant C++ implementations
of widely used metrics as well as being easily extendable to accommodate for new techniques and data formats.

Out of the box, the library supports the following set of features:

* Encryption algorithms:
    * S-Box
    * AES-128
* Models:
    * Hamming distance
    * Hamming weight
    * Identity
* Distinguishers:
    * CPA (using the Pearson correlation coefficient)
* Metrics:
    * Key rank
    * Key guess
    * Key score
    * Guessing entropy
    * Success rate
    * T-Test
    * Mutual information
    * Perceived information

As well as a library, MetriSCA comes with its own command-line tool MetriSCAcli to easily load recorded traces and output metrics in the
CSV file format.

## Building

The project supports both Windows and Linux. It uses the [premake](https://premake.github.io/) build system which is included
in the repository.

### Linux:

You can generate a Unix Makefile using the command:

```./premake5 gmake2```

The files will be located in the `build` directory.

You can compile the binary using the command:

```make -C build config=<config>```

where `<config>` can be either `debug` or `release`. The output will be located in the `bin` directory.

### Windows:

You can generate a Visual Studio 2022 solution using the command:

```premake5.exe vs2022```

You can then open the solution file from the `build` directory in Visual Studio and build the project either in `Release` or `Debug` 
configuration. The binaries can then be found in the `bin` folder.

> Note: You can also provide other versions of Visual Studio to Premake to generate the corresponding solution files. See the Premake 
> website for more info.

### Running the tests

To run the test suite, simply launch the `metrisca-tests` executable in `bin/<config>`.

## MetriSCAcli

MetriSCAcli is an interactive command-line interface that can load datasets of recorded traces and output metric results in CSV files.
Its binary is located along with the library in the `bin` folder.

### Usage

Once started from any command prompt, the list of supported commands can be obtained by typing `help`. By default, the application supports
all of the feature of the library as well as a few useful tools to manage datasets (listing, splitting, etc...).

This tool also has the ability to execute scripts instead of starting the interactive prompt. A script is simply a text file containing the 
sequential list of commands to execute. Lines that start with the character '#' are considered comments and are not executed.

### Loading a dataset

By default, the tool does not come with a way to load in datasets of recorded traces. Fortunately, it is very straight forward to create one.
Here is a code snippet describing how to write a loader for a text-based dataset format.

```C++
class TxtLoader : public LoaderPlugin {
public:
    virtual Result<void, int> Init(const ArgumentList& args) override
    {
        // Extract the necessary arguments from the command-line
        auto filename = args.GetString("filename");
        if (!filename.has_value()) {
            METRISCA_ERROR("Missing filename argument!");
            return SCA_MISSING_ARGUMENT;
        }

        m_Filename = filename.value();

        return SCA_OK;
    }

    virtual Result<void, int> Load(TraceDatasetBuilder& builder) override
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
            return SCA_FILE_NOT_FOUND;
        }

        std::string line;
        unsigned int line_count = 0;

        std::vector<int> trace;
        trace.resize(num_samples);

        while (std::getline(file, line)) {
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
            line_count++;
        }
        // The plaintexts go from 0 to 255 in order
        for(unsigned int p = 0; p < 256; ++p)
        {
            builder.AddPlaintext({(unsigned char)p});
        }
        // The key is fixed to 0
        builder.AddKey({(unsigned char)0});

        return SCA_OK;
    }

private:

    // Store some loader dependent data
    std::string m_Filename{};
};
```

Then, the loader can be registered into the application by adding this line to the `main` function.

```C++
METRISCA_REGISTER_PLUGIN(TxtLoader, "txtloader");
```

Et voilà! The loader can be used to load in your dataset.

One important note is that the performance of custom loaders is not very important as the application saves loaded datasets in an optimized format
that allows for fast loading. This means that upon restarting the application, the optimized file can be loaded instead of the original dataset for
increased performance.

In the CLI prompt, loading a dataset `dataset.txt` can be done using the command:

```
load dataset.txt data -l txtloader -o dataset.bin
```

This loads the specified filename using the previously created `txtloader` and saves the optimized representation in `dataset.bin`. The dataset can
then be referenced in the application using the `data` alias.

Loading the optimized dataset `dataset.bin` can be done using the command:

```
load dataset.bin data
```

### Computing metrics

The library provides common metrics that can be computed on datasets. Here is an example command to compute the score metric on a dataset with alias `data`:

```
metric score data -m hamming_distance -d pearson -o score.csv -b 0 -t 5000 -s 1000 -e 1200
```

This computes the score metric on byte index 0 using the *Hamming distance* model, the *Pearson correlation* distinguisher from sample 1000 to 1200 using
5000 traces. The result is finally saved in `score.csv`.

More information on the parameters of each metric can be found by using the command:

```
help metric <name>
```

## Extending the library

The library API is designed to be extended by the user. It exposes a plugin system which encapsulates most of the functions performed by the library (metrics,
loaders, power models, etc...).

A user-defined plugin must be a class that extends one of the following base classes: `LoaderPlugin`, `PowerModelPlugin`, `DistinguisherPlugin`, `MetricPlugin`
or `ProfilerPlugin`. The `METRISCA_REGISTER_PLUGIN` can then be used to register the new plugin into the library.

Plugins are constructed by the `PluginFactory::ConstructAs<T>` method which initializes a plugin using arguments passed via an `ArgumentList` object.

## License

This project is licensed under the 3-clause BSD LICENSE. See [LICENSE.md](LICENSE.md) for more information.

## Publication

For a comprehensive survey and comparison of the side-channel metrics, refer to the following article:

**The Side Channel Metrics Cheat Sheet ([pdf](https://metrisca.epfl.ch/resources/Papagiannopoulos21%20The%20side-channel%20metric%20cheat%20sheet%20(e-print).pdf))**

by Kostas Papagiannopoulos, Ognjen Glamočanin, Melissa Azouaoui, Dorian Ros, Francesco Regazzoni and Mirjana Stojilović.

## Citation

If you use MetriSCA (v1.0) and publish your results, please cite it:

```
@software{MetriSCAv1.0,
    author = {Dorian Ros and Ognjen Glamo\v{c}anin and Mirjana Stojilovi\'{c}},
    title = {{MetriSCA}: {A} Library of Metrics for Side-Channel Analysis},
    month = dec,
    year = 2021,
    publisher = {Zenodo},
    version = {v1.0},
    doi = {10.5281/zenodo.5778947},
    url = {https://doi.org/10.5281/zenodo.5778947}
}
```