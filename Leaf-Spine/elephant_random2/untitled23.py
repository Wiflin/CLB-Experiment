# -*- coding: utf-8 -*-
"""
Created on Thu May 10 18:37:55 2018

@author: Hasee-Janu
"""

import os, random, re


dict = {}
vp2path = {}
for file in os.listdir('FlowPath'):
        r = re.match('^Path\d+-(\d+)_Flow.tr$',file)
        if r:
            dict[r[1]] = []
            print(file, r[1])
            
            with open('FlowPath/'+file) as f:
                data = list(map(str.split,f.readlines()))
                ippp = '^\d+\.0-\d+\.0$' 
                for row in data:
                    if re.match(ippp,row[1]):
                        dict[r[1]].append(int(row[2]))
                        vp2path[int(row[2])] = r[1]
                
               
        

print(vp2path)                        

# record_fname = 'Record-40.tr'
# vp_record = []
# with open(record_fname,'r') as f:
#     r_src = list(map(str.split,f.readlines()))
#     for row in r_src:
#         if row[0] == '[vp_free]':
#             vp_record.append(int(row[2]))
            
# vp_record.sort()

# [print(vp2path[i]) for i in vp_record]