# MetriSCA
## Project descriptions and notes

<!--
 Boye Guillaume, Spring semester, 2023
 Semester Project: Implementation of new metric and enhancement of the MetriSCA library
 Supervisor: Shashwat Shrivastava
-->

The goal of this project is to enhance the library of side-channel evaluation metrics [MetriSCA](metrisca.epfl.ch) and the accompanying documentation.

## Milestones

* Part I (~2 week)
 * Read *The Side-Channel metric cheat sheet*
 * First view to metriSCA and enhancement of the command line user interface

### Notes and week summary

* Week 01 :
 * Setting up repository and going throw the code

* Week 02 :
 * In order to ease future key enumeration plugin add the hability
   to create more complex commands and improve command parse
  
 * CHANGES :

   * Add the ability to provide strings with spaces (for instance when your path contains spaces),
     use of the standard syntax "string with spaces". Also added the hability to create special
     character for instace string\ with\ spaces will be parsed as one single argument. Downside is
     that windows path no longer work and need to be escaped. (C:\Users... become C:\\Users...)

   * Add ability to specify multiple line commands by terminating the current line by \

   * Add new vendor `indicators` to provide more feedback to the user (and not just staring at a 
     freezed screen)

   * Starting to work on the `KeyEnumerationMetric` even though this may change in the future due to
     not having a clear view of the goal

 * Summary of 10-03-23 meeting on the project (TODO):
   * Compare the previous CPU codes with the GPU code for key rank estimation and find out the difference between them
   * Integration of CPU code on metrisca 
   * Try to optimize the CPU code so that it run faster
   * Write KeyEnumerationMetric. This metric was not what I orginally though of as the computation of the AES is parrallel (we are on FPGA). We should use a CPA based attack to compute the correlation between each sample and the model for each byte, we should then use the sample which leaks the most for a specific bytes

### Note on implementations (food for thought)

MetriSCA library uses only a single core and his heavily stages, we could introduce a more streaming like process to increase cache efficiency on MetriSCA
