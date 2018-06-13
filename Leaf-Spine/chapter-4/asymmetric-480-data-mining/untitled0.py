# -*- coding: utf-8 -*-
"""
Created on Sun Apr 15 18:33:17 2018

@author: Hasee-Janu
"""

import os

fname = os.sys.argv[1]

#fname = 'InputFlow-FCT-80-40.tr'

with open(fname) as f:
    data_src = f.readlines()
    data_src = list(map(str.split,data_src))
    data_src = [i[2] for i in data_src]
    
    data_len = len(data_src)
    time_sum = sum(map(float,data_src))
    
    time_avg = time_sum / data_len
    print(time_sum,data_len,"average time：", time_avg)
    
