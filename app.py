from flask import Flask, render_template, request, jsonify
import math
import collections

app = Flask(__name__)

class MMUSimulator:
    def __init__(self, config):
        self.ram_size = config['ram_size_kb'] * 1024
        self.page_size = config['page_size_bytes']
        self.num_frames = self.ram_size // self.page_size
        self.tlb_size = config['tlb_size']
        self.policy = config['replacement_policy']

        self.shift = int(math.log2(self.page_size))
        self.offset_mask = self.page_size - 1

        self.tlb = []                     # round‑robin TLB entries
        self.tlb_next = 0
        self.page_table = {}              # vpn -> {'frame', 'dirty', 'valid'}
        self.free_frames = list(range(self.num_frames))

        # Replacement structures
        self.fifo_queue = collections.deque()
        self.lru_list = collections.deque()   # front = MRU, back = LRU

        # Statistics
        self.total_accesses = 0
        self.tlb_hits = 0
        self.page_faults = 0
        self.dirty_writes = 0
        self.total_time_ns = 0

        # Latencies (hardcoded – can be moved to config.json if needed)
        self.tlb_lat = 1           # ns
        self.ram_lat = 100         # ns
        self.disk_lat = 10_000_000 # ns (10 ms)

    def _select_victim(self, future_uses, idx):
        if self.policy == "FIFO":
            return self.fifo_queue.popleft()
        elif self.policy == "LRU":
            return self.lru_list.pop()
        elif self.policy == "OPT":
            farthest, victim = -1, None
            for vpn in self.page_table.keys():
                while future_uses[vpn] and future_uses[vpn][0] <= idx:
                    future_uses[vpn].popleft()
                if not future_uses[vpn]:
                    return vpn
                if future_uses[vpn][0] > farthest:
                    farthest, victim = future_uses[vpn][0], vpn
            return victim

    def run(self, trace_file):
        # Precompute future uses for OPT
        future_uses = collections.defaultdict(collections.deque)
        if self.policy == "OPT":
            with open(trace_file) as f:
                for i, line in enumerate(f):
                    parts = line.split()
                    if parts:
                        addr = int(parts[-1], 16)
                        vpn = addr >> self.shift
                        future_uses[vpn].append(i)

        with open(trace_file) as f:
            for idx, line in enumerate(f):
                parts = line.split()
                if not parts:
                    continue
                op = parts[0] if len(parts) > 1 else 'R'
                addr = int(parts[-1], 16)
                vpn = addr >> self.shift

                self.total_accesses += 1

                # ----- TLB lookup -----
                hit = False
                for e in self.tlb:
                    if e['vpn'] == vpn:
                        hit = True
                        self.tlb_hits += 1
                        self.total_time_ns += self.tlb_lat + self.ram_lat
                        break

                if not hit:
                    # ----- Page table lookup -----
                    if vpn in self.page_table and self.page_table[vpn]['valid']:
                        self.total_time_ns += self.tlb_lat + self.ram_lat
                        frame = self.page_table[vpn]['frame']
                    else:
                        # ----- PAGE FAULT -----
                        self.page_faults += 1
                        self.total_time_ns += self.tlb_lat + self.ram_lat + self.disk_lat

                        if self.free_frames:
                            frame = self.free_frames.pop()
                        else:
                            # Eviction
                            victim = self._select_victim(future_uses, idx)
                            victim_info = self.page_table.pop(victim)
                            frame = victim_info['frame']
                            if victim_info['dirty']:
                                self.dirty_writes += 1
                                self.total_time_ns += self.disk_lat
                            # Remove from TLB if present
                            self.tlb = [e for e in self.tlb if e['vpn'] != victim]

                        self.page_table[vpn] = {'frame': frame, 'valid': True, 'dirty': False}
                        if self.policy == "FIFO":
                            self.fifo_queue.append(vpn)

                    # ----- Update TLB (round‑robin) -----
                    if len(self.tlb) < self.tlb_size:
                        self.tlb.append({'vpn': vpn, 'frame': frame})
                    else:
                        self.tlb[self.tlb_next] = {'vpn': vpn, 'frame': frame}
                        self.tlb_next = (self.tlb_next + 1) % self.tlb_size

                # ----- Update LRU state (on any access) -----
                if self.policy == "LRU":
                    if vpn in self.lru_list:
                        self.lru_list.remove(vpn)
                    self.lru_list.appendleft(vpn)

                # ----- Set dirty bit on write -----
                if op.upper() == 'W' and vpn in self.page_table:
                    self.page_table[vpn]['dirty'] = True

        eat = self.total_time_ns / self.total_accesses if self.total_accesses else 0
        return (f"--- PERFORMANCE REPORT ---\n"
                f"Total Accesses: {self.total_accesses}\n"
                f"TLB Hits: {self.tlb_hits}\n"
                f"Page Faults: {self.page_faults}\n"
                f"Dirty Disk Writes: {self.dirty_writes}\n"
                f"Effective Access Time (EAT): {eat:.2f} ns\n"
                f"--------------------------")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/run', methods=['POST'])
def run():
    data = request.json
    sim = MMUSimulator({
        "ram_size_kb": int(data['ram_size']),
        "page_size_bytes": int(data['page_size']),
        "tlb_size": int(data['tlb_size']),
        "replacement_policy": data['policy']
    })
    report = sim.run('test_trace_100k.txt')   # ensure this file exists
    return jsonify({"output": report})

if __name__ == '__main__':
    app.run(debug=True)