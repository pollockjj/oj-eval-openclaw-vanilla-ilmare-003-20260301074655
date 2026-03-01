#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>

using namespace std;

enum class SubmitStatus { Accepted, Wrong_Answer, Runtime_Error, Time_Limit_Exceed };

SubmitStatus parseStatus(const string& s) {
    if (s == "Accepted") return SubmitStatus::Accepted;
    if (s == "Wrong_Answer") return SubmitStatus::Wrong_Answer;
    if (s == "Runtime_Error") return SubmitStatus::Runtime_Error;
    return SubmitStatus::Time_Limit_Exceed;
}

string statusToString(SubmitStatus s) {
    switch (s) {
        case SubmitStatus::Accepted: return "Accepted";
        case SubmitStatus::Wrong_Answer: return "Wrong_Answer";
        case SubmitStatus::Runtime_Error: return "Runtime_Error";
        case SubmitStatus::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "";
}

struct ProblemStatus {
    int wrong_attempts = 0;
    bool is_solved = false;
    int solve_time = 0;
    int frozen_wrong = 0;
    int frozen_submissions = 0;
    bool is_frozen = false;
    bool has_frozen_ac = false;
    int frozen_ac_time = 0;
};

struct Team {
    string name;
    ProblemStatus problems[26];
    int current_rank = 0;
    int frozen_count = 0;
    map<pair<char, string>, pair<int, int>> last_submission;
    
    int rank_solved = 0;
    int rank_penalty = 0;
    vector<int> rank_times;
    bool rank_valid = false;
    
    int disp_solved = 0;
    int disp_penalty = 0;
    bool disp_valid = false;
    
    void updateRankStats(int pc) {
        rank_solved = 0; rank_penalty = 0; rank_times.clear();
        for (int i = 0; i < pc; i++) {
            const ProblemStatus& ps = problems[i];
            if (ps.is_solved && !ps.is_frozen) {
                rank_solved++;
                rank_penalty += 20 * ps.wrong_attempts + ps.solve_time;
                rank_times.push_back(ps.solve_time);
            }
        }
        sort(rank_times.begin(), rank_times.end(), greater<int>());
        rank_valid = true;
    }
    
    void updateDispStats(int pc) {
        disp_solved = 0; disp_penalty = 0;
        for (int i = 0; i < pc; i++) {
            const ProblemStatus& ps = problems[i];
            if (ps.is_solved) {
                disp_solved++;
                int wrong = ps.wrong_attempts;
                if (ps.is_frozen && ps.has_frozen_ac) wrong += ps.frozen_wrong;
                disp_penalty += 20 * wrong + ps.solve_time;
            }
        }
        disp_valid = true;
    }
    
    void invalidate() { rank_valid = disp_valid = false; }
};

class ICPCSystem {
    map<string, int> team_idx;
    vector<Team> teams;
    vector<int> order;
    vector<int> pos;
    bool started = false;
    int dur = 0, pc = 0;
    bool frozen = false;

    bool cmp(int a, int b) {
        Team& ta = teams[a], &tb = teams[b];
        if (!ta.rank_valid) ta.updateRankStats(pc);
        if (!tb.rank_valid) tb.updateRankStats(pc);
        if (ta.rank_solved != tb.rank_solved) return ta.rank_solved > tb.rank_solved;
        if (ta.rank_penalty != tb.rank_penalty) return ta.rank_penalty < tb.rank_penalty;
        for (size_t i = 0; i < min(ta.rank_times.size(), tb.rank_times.size()); i++)
            if (ta.rank_times[i] != tb.rank_times[i]) return ta.rank_times[i] < tb.rank_times[i];
        return ta.name < tb.name;
    }

    void doFlush() {
        for (auto& t : teams) t.invalidate();
        sort(order.begin(), order.end(), [this](int a, int b) { return cmp(a, b); });
        for (size_t i = 0; i < order.size(); i++) {
            teams[order[i]].current_rank = i + 1;
            pos[order[i]] = i;
        }
    }

    void updateOne(int idx) {
        Team& t = teams[idx];
        t.invalidate();
        t.updateRankStats(pc);
        
        int cur = pos[idx], n = order.size();
        int lo = 0, hi = n;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (cmp(idx, order[mid])) hi = mid;
            else lo = mid + 1;
        }
        
        if (lo != cur) {
            order.erase(order.begin() + cur);
            if (lo > cur) lo--;
            order.insert(order.begin() + lo, idx);
            for (int i = max(0, lo - 1); i < min(n, lo + 2); i++)
                pos[order[i]] = i;
        }
        for (size_t i = 0; i < order.size(); i++)
            teams[order[i]].current_rank = i + 1;
    }

    string getBoard() {
        doFlush();
        stringstream ss;
        for (int idx : order) {
            Team& t = teams[idx];
            if (!t.disp_valid) t.updateDispStats(pc);
            ss << t.name << " " << t.current_rank << " " << t.disp_solved << " " << t.disp_penalty;
            for (int i = 0; i < pc; i++) {
                ss << " ";
                auto& ps = t.problems[i];
                if (ps.is_frozen) {
                    if (ps.wrong_attempts == 0) ss << "0/" << ps.frozen_submissions;
                    else ss << "-" << ps.wrong_attempts << "/" << ps.frozen_submissions;
                } else if (ps.is_solved) {
                    ss << (ps.wrong_attempts == 0 ? "+" : "+" + to_string(ps.wrong_attempts));
                } else {
                    ss << (ps.wrong_attempts == 0 ? "." : "-" + to_string(ps.wrong_attempts));
                }
            }
            ss << "\n";
        }
        return ss.str();
    }

public:
    string addTeam(const string& name) {
        if (started) return "[Error]Add failed: competition has started.\n";
        if (team_idx.count(name)) return "[Error]Add failed: duplicated team name.\n";
        int idx = teams.size();
        teams.push_back(Team());
        teams.back().name = name;
        team_idx[name] = idx;
        order.push_back(idx);
        pos.push_back(idx);
        return "[Info]Add successfully.\n";
    }

    string start(int duration, int problems) {
        if (started) return "[Error]Start failed: competition has started.\n";
        started = true; dur = duration; pc = problems;
        sort(order.begin(), order.end(), [this](int a, int b) { return teams[a].name < teams[b].name; });
        for (size_t i = 0; i < order.size(); i++) {
            teams[order[i]].current_rank = i + 1;
            pos[order[i]] = i;
        }
        return "[Info]Competition starts.\n";
    }

    void submit(const string& prob, const string& team, const string& st, int time) {
        Team& t = teams[team_idx[team]];
        int p = prob[0] - 'A';
        auto status = parseStatus(st);
        auto& ps = t.problems[p];
        t.last_submission[{prob[0], statusToString(status)}] = {time, (int)t.last_submission.size()};
        
        if (frozen && !ps.is_solved) {
            ps.frozen_submissions++;
            if (status == SubmitStatus::Accepted) {
                ps.has_frozen_ac = true; ps.frozen_ac_time = time;
            } else ps.frozen_wrong++;
            if (!ps.is_frozen) { ps.is_frozen = true; t.frozen_count++; }
        } else {
            if (status == SubmitStatus::Accepted && !ps.is_solved) {
                ps.is_solved = true; ps.solve_time = time;
            } else if (status != SubmitStatus::Accepted && !ps.is_solved) {
                ps.wrong_attempts++;
            }
            t.invalidate();
        }
    }

    string flush() { doFlush(); return "[Info]Flush scoreboard.\n"; }
    string freeze() {
        if (frozen) return "[Error]Freeze failed: scoreboard has been frozen.\n";
        frozen = true; return "[Info]Freeze scoreboard.\n";
    }

    string scroll() {
        if (!frozen) return "[Error]Scroll failed: scoreboard has not been frozen.\n";
        stringstream out;
        out << "[Info]Scroll scoreboard.\n" << getBoard();
        
        map<int, int> init_rank, rank2idx;
        for (int idx : order) {
            init_rank[idx] = teams[idx].current_rank;
            rank2idx[teams[idx].current_rank] = idx;
        }
        
        set<pair<int, int>> pq; // (-rank, idx)
        for (int idx : order) if (teams[idx].frozen_count > 0) pq.insert({-teams[idx].current_rank, idx});
        
        while (!pq.empty()) {
            auto it = *pq.begin(); pq.erase(pq.begin());
            int idx = it.second;
            Team& t = teams[idx];
            int prob = -1;
            for (int i = 0; i < pc; i++) if (t.problems[i].is_frozen) { prob = i; break; }
            if (prob < 0) continue;
            
            auto& ps = t.problems[prob];
            int ir = init_rank[idx];
            
            if (ps.has_frozen_ac) {
                ps.is_solved = true; ps.solve_time = ps.frozen_ac_time;
                ps.wrong_attempts += ps.frozen_wrong;
            } else ps.wrong_attempts += ps.frozen_wrong;
            
            ps.is_frozen = ps.has_frozen_ac = false;
            ps.frozen_submissions = ps.frozen_wrong = 0;
            t.frozen_count--;
            t.invalidate();
            
            if (ps.is_solved) {
                int old_rank = t.current_rank;
                updateOne(idx);
                if (t.current_rank < ir) {
                    int disp = rank2idx[t.current_rank];
                    if (disp != idx) {
                        if (!t.disp_valid) t.updateDispStats(pc);
                        out << t.name << " " << teams[disp].name << " " << t.disp_solved << " " << t.disp_penalty << "\n";
                    }
                }
            }
            
            if (t.frozen_count > 0) pq.insert({-t.current_rank, idx});
        }
        
        out << getBoard();
        frozen = false;
        return out.str();
    }

    string queryRank(const string& name) {
        if (!team_idx.count(name)) return "[Error]Query ranking failed: cannot find the team.\n";
        stringstream out;
        out << "[Info]Complete query ranking.\n";
        if (frozen) out << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        out << name << " NOW AT RANKING " << teams[team_idx[name]].current_rank << "\n";
        return out.str();
    }

    string querySub(const string& name, const string& prob, const string& st) {
        if (!team_idx.count(name)) return "[Error]Query submission failed: cannot find the team.\n";
        Team& t = teams[team_idx[name]];
        stringstream out;
        out << "[Info]Complete query submission.\n";
        char p = (prob == "ALL") ? 0 : prob[0];
        int bt = -1, bi = -1; char rp = 0; string rs;
        for (auto& kv : t.last_submission) {
            const auto& k = kv.first;
            const auto& v = kv.second;
            bool pm = (p == 0) || (k.first == p);
            bool sm = (st == "ALL") || (k.second == st);
            if (pm && sm && (v.first > bt || (v.first == bt && v.second > bi))) {
                bt = v.first; bi = v.second; rp = k.first; rs = k.second;
            }
        }
        if (bt >= 0) out << name << " " << rp << " " << rs << " " << bt << "\n";
        else out << "Cannot find any submission.\n";
        return out.str();
    }

    string end() { return "[Info]Competition ends.\n"; }
};

int main() {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    ICPCSystem sys;
    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        string cmd; iss >> cmd;
        if (cmd == "ADDTEAM") { string n; iss >> n; cout << sys.addTeam(n); }
        else if (cmd == "START") { string d1, d2; int du, pr; iss >> d1 >> du >> d2 >> pr; cout << sys.start(du, pr); }
        else if (cmd == "SUBMIT") { string p, b, t, w, s, a; int tm; iss >> p >> b >> t >> w >> s >> a >> tm; sys.submit(p, t, s, tm); }
        else if (cmd == "FLUSH") cout << sys.flush();
        else if (cmd == "FREEZE") cout << sys.freeze();
        else if (cmd == "SCROLL") cout << sys.scroll();
        else if (cmd == "QUERY_RANKING") { string n; iss >> n; cout << sys.queryRank(n); }
        else if (cmd == "QUERY_SUBMISSION") { string n, w1, pc, a1, sc; iss >> n >> w1 >> pc >> a1 >> sc; size_t e = pc.find('='); string p = pc.substr(e+1); e = sc.find('='); string s = sc.substr(e+1); cout << sys.querySub(n, p, s); }
        else if (cmd == "END") { cout << sys.end(); break; }
    }
    return 0;
}
