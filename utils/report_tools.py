import os
import csv
import pdb

import pandas as pd
import matplotlib.pyplot as plt

class Experiment:
    columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v"]

    def __init__(self, exp_msg_count):
        self._stats={}
        self._data={}
        self._df={}
        self._ts_ordered_data={}
        self._total_msg_count=exp_msg_count

    def importExpDir(self, dir_path):
        if not os.path.isdir(dir_path):
            raise OSError("No such directory")
        for filename in os.listdir(dir_path):
            file_path = dir_path + filename
            #TODO Retrieve group id from filename aswell
            node_id = int(filename.split(".")[1])
            self._importExpReportFile(file_path, node_id)
            self._df[node_id] = pd.read_table(file_path, names=self.columns)
            print(filename, "imported")

    def check(self):
        #Check whether the number of messages is OK
        if len(self._data.keys()) != self._total_msg_count:
            return False
        for msg in self._data.values():
            #TODO Check whether the number of groups is OK
            #TODO Check whether all groups are in destgrp(msg)
            #TODO Check whether the number of nodes for that group is OK
            #Check whether msg data is the same for all nodes
            all_msg_data = [ { k: v for k,v in node.items() if k in ["mid", "gts", "ngrp", "grps", "pl_l", "pl_v"] } for node in msg.values() ]
            if all_msg_data and not all_msg_data.count(all_msg_data[0]) == len(all_msg_data):
                return False
        return True

    def checkDF(self):
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

    def computeStats(self):
        gts_ordered_data = sorted(self._data.items())
        ts_ordered_data = { k: v for k,v in self._data.items() }
        first_ts={}
        for nid,node in self._ts_ordered_data.items():
            first_ts[nid] = min( [ msg.get("ts_start") for msg in sorted(node.values()) ] )
            for ts_end,msg in sorted(node.items()):
        #for gts,msg in gts_ordered_data:
            #for nid,node in msg.items():
                cur_stats = self._stats.setdefault(nid, {}).setdefault(ts_end, {});
                if len(self._stats[nid]) == 1:
                    cur_stats["ind"] = 1
                    cur_stats["lat"] = msg.get("ts_end") - msg.get("ts_start")
                    cur_stats["lat_min"], cur_stats["lat_max"], cur_stats["lat_avg"] = [ cur_stats["lat"] ] * 3
                else:
                    prev_stats = lambda stat_key: self._stats.get(nid).get(prev).get(stat_key)
                    cur_stats["ind"] = prev_stats("ind") + 1
                    cur_stats["lat"] = msg.get("ts_end") - msg.get("ts_start")
                    cur_stats["lat_min"] = min(cur_stats["lat"], prev_stats("lat_min"))
                    cur_stats["lat_max"] = max(cur_stats["lat"], prev_stats("lat_max"))
                    cur_stats["lat_avg"] = prev_stats("lat_avg") + ((cur_stats["lat"] - prev_stats("lat_avg")) / (cur_stats["ind"]))
                    cur_stats["msg_per_sec"] = ( cur_stats["ind"] ) / ( msg.get("ts_end") - first_ts[nid] )
                prev = msg

    #With only one time capture per msg, the stats are not relevent
    def computeStatsDf(self):
        for nid,df in self._df.items():
            #Create a new df from this node data df by droping irrelevent columns and sorting by ts
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v']).sort_values(by=['gts','mid']).reset_index(level=0, drop=True)
            #Add extra latency column as the diff of a row's ts with previous one's ts
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["lat_min"], stats["lat_max"], stats["lat_avg"] = stats["lat"].expanding().min() , stats["lat"].expanding().max(), stats["lat"].expanding().mean()
            #Add extra throughput column as row's index divided by SUM(lat) up to that row
            ts_exp_start = stats["ts_start"].min()
            stats["msgps"] = stats.index / ( stats["ts_end"] - ts_exp_start )
            #Add this new df to _stats container
            self._stats[nid] = stats
        return

    def plot(self):
        return

    def _importExpReportFile(self, file_path, node_id):
        with open(file_path) as report_file:
            reader = csv.DictReader(report_file, fieldnames=self._columns, dialect="excel-tab")
            for row in reader:
                self._data.setdefault(eval(row.get("gts")), {}).setdefault(node_id, {}).update( { k: eval(v) if k != "pl_v" else v for k, v in row.items() } )
                self._ts_ordered_data.setdefault(node_id, {}).setdefault(eval(row.get("ts_end")), {}).update( { k: eval(v) if k != "pl_v" else v for k,v in row.items() } )

if __name__ == "__main__":
    exp = Experiment(200000)
    exp.importExpDir("/home/anatole/log/")

    if not exp.check():
        raise Exception("FAILURE: report files are not consistent")

    exp.computeStats()

    pdb.set_trace()
