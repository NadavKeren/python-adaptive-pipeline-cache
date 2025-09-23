A simple implemetation of the Adaptive Pipeline Cache.
Currently planned supporting FIFO, ALRU, ATinyLFU and ACA-LFU

The implementation follows the interface of cacheutils for experiments in GPT-Cache.

Building is as follows:
>> cmake .
>> make
>> python setup.py build_ext --inplace