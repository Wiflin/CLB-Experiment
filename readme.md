![welcome](https://github.com/Wiflin/CLB-Experiment/raw/master/wel.png)

-------------

## # Todoist

1. ECMP Part : 
	- ~~Change to a new **Expansible** Leaf-Spine Top Structure~~

2. Per-Packet Part : 
	- Change **Duplicate Threshold** to accommodate the re-order packets

3. Flowlet Part : 
	- In the NS2 Simulator, Flowlet only appear on lossy link (cwnd is not large), I have to **make some lose on the link artifficially**
	- how to **choose a approximate interval** between two flowlet, it should be **static or dynamic**
	- how to **choose the new route**, it should be **random or based on some algorithm**


## # CLB

1. different network status
	- high speed & low latency
	- high speed & high latency
	- low speed & low latency
	- low speed & high latency

2. different user scene
	- data mining
	- web search