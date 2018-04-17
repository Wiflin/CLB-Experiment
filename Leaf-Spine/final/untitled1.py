# -*- coding: utf-8 -*-
"""
Created on Tue Apr 17 00:57:06 2018

@author: Hasee-Janu

plot FCT to a png
"""

import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

fname = os.sys.argv[1]

#fname = 'InputFlow-FCT-80-40.tr'

plt.figure(dpi=100)

with open(fname) as f:
    data_src = f.readlines()
    data_src = list(map(str.split,data_src))
    data_src = [i[2] for i in data_src]
    
    data_len = len(data_src)
    time = list(map(float,data_src))
 #   time.sort()
   
    xs = list(range(len(time)))
    plt.scatter(xs,time,s=3,color='red')
    time.sort()    
    # plt.plot(xs,time,color='blue')

    plt.savefig('FCT-Distribution')
    
   
    