# -*- coding: utf-8 -*-
"""
Created on Tue Apr 17 01:14:02 2018

@author: Hasee-Janu
"""

import os, random

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

fname = os.sys.argv[1]

#fname = 'Flowid-37.tr'

servers = os.sys.argv[2] if len(os.sys.argv) > 2 else 80
accesss = os.sys.argv[3] if len(os.sys.argv) > 3 else  2
tops = os.sys.argv[4] if len(os.sys.argv) > 4 else 4

offset = 2001

plt.figure(dpi = 250)

with open(fname) as f:
    data_src = f.readlines()
    data_src = list(map(str.split,data_src))
#    data_src = [i[2] for i in data_src]
    

    for i in data_src:
        i.extend(str.split(i[1],'-'))
        i[0] = float(i[0]) - offset
        i[2] = int(i[2])
        i[3] = int(i[3])    # last hop
        i[4] = int(i[4])    # next hop
    
    plt.xlim((-0.0003,data_src[-1][0]))
    plt.ylim((servers-1,servers+accesss+tops))
    
    for i in data_src:
        # from leaf to spine
        if(i[2] > 1000 and i[3] >= servers and i[3] < servers + accesss):
            plt.scatter(i[0],
                        (i[4] + (random.random()-0.5) * 0.3),
                        s=0.5,
                        color='blue')
        # from spine to dst leaf
        if(i[2] > 1000 and i[3] >= servers+accesss and 
           i[4] >= servers and i[4] < servers + accesss):
            plt.scatter(i[0],
                        (i[4] + (random.random()-0.5) * 0.3),
                        s=0.5,
                        color='green')
        # from src to leaf  
        if(i[2] > 1000 and i[3] < servers and 
           i[4] >= servers and i[4] < servers + accesss):
            plt.scatter(i[0],
                        (i[4] + (random.random()-0.5)),
                        s=0.5,
                        color='orange')

        # from leaf to dst
    #    if(i[2] > 1000 and i[3] >= servers and i[3] < servers + accesss and
     #      i[4] < servers):
      #      plt.scatter(i[0],
       #                 (i[3] - 0.2 + (random.random()-0.5) * 0.1),
        #                s=0.5,
         #               color='magenta')
            
        # ack from dst to leaf
        if(i[2] < 150 and i[3] < servers and 
           i[4] >= servers and i[4] < servers + accesss):
            plt.scatter(i[0],
                        (i[4] + 0.2 + (random.random()-0.5) * 0.1),
                        s=0.5,
                        color='red')
        
            
        # ack from top to leaf
        if(i[2] < 150 and i[3] >= servers + accesss and 
           i[4] >= servers and i[4] < servers + accesss):
            plt.scatter(i[0],
                        (i[4] + (random.random()-0.5)),
                        s=0.5,
                        color='red')
            
            
    
    
plt.savefig(fname+'.png')
