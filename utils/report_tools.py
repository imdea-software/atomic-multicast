import os
import pdb

import pandas as pd
import matplotlib.pyplot as plt

class Experiment:
    columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v"]

    def __init__(self, exp_msg_count):
        self._stats={}
        self._df={}
        self._total_msg_count=exp_msg_count

    def importDir(self, dir_path):
        if not os.path.isdir(dir_path):
            raise OSError("No such directory")
        for filename in os.listdir(dir_path):
            file_path = dir_path + filename
            #TODO Retrieve group id from filename aswell
            node_id = int(filename.split(".")[1])
            self._df[node_id] = pd.read_table(file_path, names=self.columns)
            print(filename, "imported")

    def check(self):
        big_df = pd.concat(self._df, names=["nid"]).drop(columns=["ts_start", "ts_end"]).reset_index(level=["nid"])
        #Check for each node whether the number of messages is OK
        if not big_df.groupby("nid")["mid"].count().eq(self._total_msg_count).all():
                return False
        #Check for each node whether the number of distinct gts is OK
        if not big_df.groupby("nid")["gts"].nunique().eq(self._total_msg_count).all():
                return False
        #Check whether msg data is the same for all nodes involved
        if not big_df.drop("nid", axis=1).groupby("mid").nunique().eq(1).all().all():
            return False
        #TODO Check whether the number of groups is OK
        #TODO Check whether all groups are in destgrp(msg)
        #TODO Check whether the number of nodes for that group is OK
        return True

    #With only one time capture per msg, the stats are not relevent
    def computeStats(self):
        for nid,df in self._df.items():
            #Create a new df from this node data df by droping irrelevent columns and sorting by ts
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v']).applymap(lambda x: eval(x) if type(x) is str else x).sort_values(by=['gts','mid']).reset_index(level=0, drop=True)
            #Add extra latency column as the diff of a row's ts with previous one's ts
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["lat_min"], stats["lat_max"], stats["lat_avg"] = stats["lat"].expanding().min() , stats["lat"].expanding().max(), stats["lat"].expanding().mean()
            #Add extra throughput column as row's index divided by SUM(lat) up to that row
            ts_exp_start = stats["ts_start"].min()
            stats["msgps"] = stats.index / ( stats["ts_end"] - ts_exp_start )
            #Add extra time column as offset-ed ts_start for easier reading
            stats["t"] = stats["ts_start"] - ts_exp_start
            #Add this new df to _stats container
            self._stats[nid] = stats
        return

    def plot(self):
        fig, axes = plt.subplots(len(self._stats.keys()), 2, figsize=(6,6))
        for nid,stats in self._stats.items():
            stats.plot(x="t", y=["lat", "lat_avg"], ax=axes[nid][0])
            stats.plot(x="t", y=["msgps"], ax=axes[nid][1])
            axes
        axes[0][0].set_title("Latency (s)")
        axes[0][1].set_title("Throughput (msg/s)")
        plt.show()
        return

if __name__ == "__main__":
    exp = Experiment(200000)
    exp.importDir("/home/anatole/log/")

    if not exp.check():
        raise Exception("FAILURE: report files are not consistent")

    exp.computeStats()

    exp.plot()

    pdb.set_trace()
