# -*- coding: utf-8 -*-
"""
Created on Sun Apr 15 18:33:17 2018

@author: Hasee-Janu
"""

import os


method = ["overview","small","mid","large"][3]

fname = os.sys.argv[1]

if len(os.sys.argv) == 3:
    method = os.sys.argv[2]


#fname = 'InputFlow-FCT-64-640.tr'

with open(fname) as f:
    data_src = f.readlines()
    data_src = list(map(
            lambda x: list(map(float,x.split()))
            ,data_src
            ))
    
    data_filtered = []
    time_sum = 0
    
    for row in data_src:
        if((method == 'overview') or
           (method == 'small' and row[1] < 100 * 1000) or
           (method == 'mid' and row[1] < 10 * 1000 * 1000 and row[1] >= 100 * 1000) or
           (method == 'large' and row[1] >= 10 * 1000 * 1000)
           ):
            data_filtered.append(row)
            time_sum += row[2]
            

    data_len = len(data_filtered)
    time_avg = time_sum / data_len

    
    print(time_sum,data_len,"%s average-time：" % (method), time_avg)