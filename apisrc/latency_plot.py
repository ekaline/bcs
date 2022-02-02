#!/usr/bin/python3.6

import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 2:
    print("not enough arguments")
    sys.exit(1)

    
f = open(sys.argv[1], "r")
lines = f.readlines()
f.close()
latencies = sorted(map(lambda x: int(x.split(",")[1]), lines))

latencies = latencies[:len(latencies) * 95 // 100] #
###plt.plot(range(len(latencies)), latencies)

plt.hist(latencies, 1000)
plt.xlabel("Latency")
plt.ylabel("Messages")
plt.show()
