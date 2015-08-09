# canhack
Taking the previous work from the toyothack repository and applying it to the 2015 Honda Fit, and also more generic applications.

###canhack.c
Contains Honda-specific decoding of CAN frames for the 2015 Honda Fit.  Contains a couple modifications to support extended can frame decoding.

###canwatch.c
Based on canhack.c code and allows a user to watch the raw data on one CAN ID in different data representations.  Currently supported is hex, decimal, and ASCII.
