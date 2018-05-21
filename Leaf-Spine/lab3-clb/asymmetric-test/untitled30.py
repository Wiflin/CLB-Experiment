# -*- coding: utf-8 -*-
"""
Created on Fri May 18 13:04:27 2018

@author: Hasee-Janu
"""

import matplotlib.pyplot as plt
import os

fname=os.sys.argv[1]


with open(fname, 'r') as f:
    data_src = list(map(str.strip,f.readlines()))


    with open(fname+'.r','w') as fw:
    
        cnt = 0
    
        for row in data_src:
            
            
            cnt += 1
            
            if cnt % 1000 :
                continue
            
            
            print(row, file=fw)
            
        
        