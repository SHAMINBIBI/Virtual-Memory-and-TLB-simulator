from flask import Flask, render_template, request, jsonify
import json
import math
import collections

app = Flask(__name__)

# --- MMU LOGIC PORTED TO PYTHON ---
class MMUSimulator:
    def __init__(self, config):
        self.ram_size = config['ram_size_kb'] * 1024
        self.page_size = config['page_size_bytes']
        self.num_frames = self.ram_size // self.page_size
        self.tlb_size = config['tlb_size']
        self.policy = config['replacement_policy']
        
        self.shift_amount = int(math.log2(self.page_size))
        self.offset_mask = self.page_size - 1
        
        self.tlb = [] # List of {'vpn': x, 'frame': y}
        self.page_table = {} # vpn -> {'frame': x, 'dirty': bool, 'valid': bool}
        self.free_frames = list(range(self.num_frames))
        
        # Replacement Structures
        self.fifo_queue = collections.deque()
        self.lru_list = collections.deque() # Front is MRU, Back is LRU
        
        # Stats
        self.total_accesses = 0
        self.tlb_hits = 0
        self.page_faults = 0
        self.dirty_writes = 0
        self.total_time_ns = 0
        
        self.tlb_lat = config['latencies']['tlb_ns']
        self.ram_lat = config['latencies']['ram_ns']
        self.disk_lat = config['latencies']['disk_ms'] * 1_000_000

    def run(self, trace_file):
        with open(trace_file, 'r') as f:
            lines = f.readlines()
            
        # For OPT algorithm
        future_uses = collections.defaultdict(collections.deque)
        if self.policy == "OPT":
            for i, line in enumerate(lines):
                parts = line.split()
                if not parts: continue
                addr = int(parts[-1], 16)
                vpn = addr >> self.shift_amount
                future_uses[vpn].append(i)

        for i, line in enumerate(lines):
            parts = line.split()
            if not parts: continue
            op = parts[0] if len(parts) > 1 else 'R'
            addr = int(parts[-1], 16)
            vpn = addr >> self.shift_amount
            
            self.total_accesses += 1
            
            # 1. TLB Lookup
            tlb_hit = False
            for entry in self.tlb:
                if entry['vpn'] == vpn:
                    tlb_hit = True
                    self.tlb_hits += 1
                    self.total_time_ns += (self.tlb_lat + self.ram_lat)
                    break
            
            if not tlb_hit:
                # 2. Page Table Lookup
                if vpn in self.page_table and self.page_table[vpn]['valid']:
                    self.total_time_ns += (self.tlb_lat + self.ram_lat)
                    frame = self.page_table[vpn]['frame']
                else:
                    # 3. PAGE FAULT
                    self.page_faults += 1
                    self.total_time_ns += (self.tlb_lat + self.ram_lat + self.disk_lat)
                    
                    if self.free_frames:
                        frame = self.free_frames.pop(0)
                    else:
                        # EVICTION
                        victim_vpn = self.select_victim(future_uses, i)
                        victim_info = self.page_table.pop(victim_vpn)
                        frame = victim_info['frame']
                        if victim_info['dirty']:
                            self.dirty_writes += 1
                            self.total_time_ns += self.disk_lat
                        
                        # Invalidate TLB if victim was there
                        self.tlb = [e for e in self.tlb if e['vpn'] != victim_vpn]
                    
                    self.page_table[vpn] = {'frame': frame, 'valid': True, 'dirty': False}
                    self.fifo_queue.append(vpn)
                
                # Update TLB (Round Robin replacement)
                if len(self.tlb) >= self.tlb_size:
                    self.tlb.pop(0)
                self.tlb.append({'vpn': vpn, 'frame': self.page_table[vpn]['frame']})

            # Update Replacement State
            if self.policy == "LRU":
                if vpn in self.lru_list: self.lru_list.remove(vpn)
                self.lru_list.appendleft(vpn)
            
            if op.upper() == 'W':
                self.page_table[vpn]['dirty'] = True

        eat = self.total_time_ns / self.total_accesses if self.total_accesses > 0 else 0
        return (f"--- PERFORMANCE REPORT ---\n"
                f"Total Accesses: {self.total_accesses}\n"
                f"TLB Hits: {self.tlb_hits}\n"
                f"Page Faults: {self.page_faults}\n"
                f"Dirty Disk Writes: {self.dirty_writes}\n"
                f"Effective Access Time (EAT): {eat:.2f} ns\n"
                f"--------------------------")

    def select_victim(self, future_uses, current_idx):
        if self.policy == "FIFO":
            return self.fifo_queue.popleft()
        elif self.policy == "LRU":
            return self.lru_list.pop()
        elif self.policy == "OPT":
            farthest_use = -1
            victim = None
            for vpn in self.page_table.keys():
                while future_uses[vpn] and future_uses[vpn][0] <= current_idx:
                    future_uses[vpn].popleft()
                
                if not future_uses[vpn]:
                    return vpn # Not used again
                
                if future_uses[vpn][0] > farthest_use:
                    farthest_use = future_uses[vpn][0]
                    victim = vpn
            return victim

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/run', methods=['POST'])
def run_simulation():
    try:
        data = request.json
        sim = MMUSimulator({
            "ram_size_kb": int(data['ram_size']),
            "page_size_bytes": int(data['page_size']),
            "tlb_size": int(data['tlb_size']),
            "replacement_policy": data['policy'],
            "latencies": {"tlb_ns": 1, "ram_ns": 100, "disk_ms": 10}
        })
        
        report = sim.run('test_trace_100k.txt')
        return jsonify({"success": True, "output": report})
    except Exception as e:
        return jsonify({"success": False, "output": str(e)})

if __name__ == '__main__':
    app.run(debug=True)