#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <tuple>

using namespace std;

enum class SubmitStatus {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

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
    ProblemStatus problems[26];  // Fixed array for faster access
    int current_rank = 0;
    int frozen_count = 0;  // Number of frozen problems
    map<pair<char, string>, pair<int, int>> last_submission;
};

class ICPCSystem {
private:
    map<string, int> team_index;
    vector<Team> team_list;
    vector<string> team_names_sorted;  // Sorted by rank
    bool competition_started = false;
    int duration_time = 0;
    int problem_count = 0;
    bool is_frozen = false;
    
    tuple<int, int, vector<int>> getRankingStats(const Team& team) const {
        int solved = 0, penalty = 0;
        vector<int> times;
        
        for (int i = 0; i < problem_count; i++) {
            const ProblemStatus& ps = team.problems[i];
            if (ps.is_solved && !ps.is_frozen) {
                solved++;
                penalty += 20 * ps.wrong_attempts + ps.solve_time;
                times.push_back(ps.solve_time);
            }
        }
        sort(times.begin(), times.end(), greater<int>());
        return {solved, penalty, times};
    }
    
    pair<int, int> getDisplayStats(const Team& team) const {
        int solved = 0, penalty = 0;
        
        for (int i = 0; i < problem_count; i++) {
            const ProblemStatus& ps = team.problems[i];
            if (ps.is_solved) {
                solved++;
                int wrong = ps.wrong_attempts;
                if (ps.is_frozen && ps.has_frozen_ac) {
                    wrong += ps.frozen_wrong;
                }
                penalty += 20 * wrong + ps.solve_time;
            }
        }
        return {solved, penalty};
    }
    
    bool compareTeams(int a_idx, int b_idx) const {
        const Team& a = team_list[a_idx];
        const Team& b = team_list[b_idx];
        
        auto [a_solved, a_penalty, a_times] = getRankingStats(a);
        auto [b_solved, b_penalty, b_times] = getRankingStats(b);
        
        if (a_solved != b_solved) return a_solved > b_solved;
        if (a_penalty != b_penalty) return a_penalty < b_penalty;
        
        for (size_t i = 0; i < min(a_times.size(), b_times.size()); i++) {
            if (a_times[i] != b_times[i]) return a_times[i] < b_times[i];
        }
        
        return a.name < b.name;
    }
    
    void flushScoreboard() {
        sort(team_names_sorted.begin(), team_names_sorted.end(), 
             [this](const string& a, const string& b) { 
                 return compareTeams(team_index[a], team_index[b]); 
             });
        
        for (size_t i = 0; i < team_names_sorted.size(); i++) {
            team_list[team_index[team_names_sorted[i]]].current_rank = i + 1;
        }
    }
    
    // Quick update for single team - insert at correct position
    void updateTeamRank(int team_idx) {
        Team& target = team_list[team_idx];
        string name = target.name;
        
        // Find current position
        int current_pos = -1;
        for (size_t i = 0; i < team_names_sorted.size(); i++) {
            if (team_names_sorted[i] == name) {
                current_pos = i;
                break;
            }
        }
        
        if (current_pos < 0) return;
        
        // Remove from current position
        team_names_sorted.erase(team_names_sorted.begin() + current_pos);
        
        // Find new position (binary search)
        int lo = 0, hi = team_names_sorted.size();
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (compareTeams(team_idx, team_index[team_names_sorted[mid]])) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }
        
        // Insert at new position
        team_names_sorted.insert(team_names_sorted.begin() + lo, name);
        
        // Update all ranks
        for (size_t i = 0; i < team_names_sorted.size(); i++) {
            team_list[team_index[team_names_sorted[i]]].current_rank = i + 1;
        }
    }
    
    string getScoreboard() {
        flushScoreboard();
        
        stringstream ss;
        for (const auto& name : team_names_sorted) {
            const Team& team = team_list[team_index[name]];
            auto [solved, penalty] = getDisplayStats(team);
            
            ss << name << " " << team.current_rank << " " << solved << " " << penalty;
            
            for (int i = 0; i < problem_count; i++) {
                ss << " ";
                const ProblemStatus& ps = team.problems[i];
                if (ps.is_frozen) {
                    if (ps.wrong_attempts == 0) {
                        ss << "0/" << ps.frozen_submissions;
                    } else {
                        ss << "-" << ps.wrong_attempts << "/" << ps.frozen_submissions;
                    }
                } else if (ps.is_solved) {
                    if (ps.wrong_attempts == 0) {
                        ss << "+";
                    } else {
                        ss << "+" << ps.wrong_attempts;
                    }
                } else {
                    if (ps.wrong_attempts == 0) {
                        ss << ".";
                    } else {
                        ss << "-" << ps.wrong_attempts;
                    }
                }
            }
            ss << "\n";
        }
        return ss.str();
    }
    
public:
    string addTeam(const string& name) {
        if (competition_started) {
            return "[Error]Add failed: competition has started.\n";
        }
        if (team_index.count(name)) {
            return "[Error]Add failed: duplicated team name.\n";
        }
        int idx = team_list.size();
        team_list.push_back(Team());
        team_list.back().name = name;
        team_index[name] = idx;
        team_names_sorted.push_back(name);
        return "[Info]Add successfully.\n";
    }
    
    string startCompetition(int duration, int problems) {
        if (competition_started) {
            return "[Error]Start failed: competition has started.\n";
        }
        competition_started = true;
        duration_time = duration;
        problem_count = problems;
        
        sort(team_names_sorted.begin(), team_names_sorted.end());
        for (size_t i = 0; i < team_names_sorted.size(); i++) {
            team_list[team_index[team_names_sorted[i]]].current_rank = i + 1;
        }
        
        return "[Info]Competition starts.\n";
    }
    
    void submit(const string& problem_name, const string& team_name, 
                const string& submit_status, int time) {
        Team& team = team_list[team_index[team_name]];
        int p = problem_name[0] - 'A';
        SubmitStatus status = parseStatus(submit_status);
        string status_str = statusToString(status);
        
        ProblemStatus& ps = team.problems[p];
        
        team.last_submission[{problem_name[0], status_str}] = {time, team.last_submission.size()};
        
        if (is_frozen && !ps.is_solved) {
            ps.frozen_submissions++;
            if (status == SubmitStatus::Accepted) {
                ps.has_frozen_ac = true;
                ps.frozen_ac_time = time;
            } else {
                ps.frozen_wrong++;
            }
            if (!ps.is_frozen) {
                ps.is_frozen = true;
                team.frozen_count++;
            }
        } else {
            if (status == SubmitStatus::Accepted && !ps.is_solved) {
                ps.is_solved = true;
                ps.solve_time = time;
            } else if (status != SubmitStatus::Accepted && !ps.is_solved) {
                ps.wrong_attempts++;
            }
        }
    }
    
    string flush() {
        flushScoreboard();
        return "[Info]Flush scoreboard.\n";
    }
    
    string freeze() {
        if (is_frozen) {
            return "[Error]Freeze failed: scoreboard has been frozen.\n";
        }
        is_frozen = true;
        return "[Info]Freeze scoreboard.\n";
    }
    
    string scroll() {
        if (!is_frozen) {
            return "[Error]Scroll failed: scoreboard has not been frozen.";
        }
        
        stringstream output;
        output << "[Info]Scroll scoreboard.\n";
        output << getScoreboard();
        
        vector<string> ranking_changes;
        
        // Initial rankings
        map<string, int> initial_rank;
        map<int, string> rank_to_team;
        for (const auto& name : team_names_sorted) {
            int rank = team_list[team_index[name]].current_rank;
            initial_rank[name] = rank;
            rank_to_team[rank] = name;
        }
        
        // Build list of teams with frozen problems
        vector<string> teams_with_frozen;
        for (const auto& name : team_names_sorted) {
            if (team_list[team_index[name]].frozen_count > 0) {
                teams_with_frozen.push_back(name);
            }
        }
        
        while (!teams_with_frozen.empty()) {
            // Find lowest-ranked team with frozen problems
            int lowest_rank = -1;
            int target_idx = -1;
            int target_problem = -1;
            
            for (size_t i = 0; i < teams_with_frozen.size(); i++) {
                Team& team = team_list[team_index[teams_with_frozen[i]]];
                int rank = team.current_rank;
                if (rank <= lowest_rank) continue;
                
                // Find smallest frozen problem
                for (int j = 0; j < problem_count; j++) {
                    if (team.problems[j].is_frozen) {
                        lowest_rank = rank;
                        target_idx = i;
                        target_problem = j;
                        break;
                    }
                }
            }
            
            if (target_idx < 0) break;
            
            string target_name = teams_with_frozen[target_idx];
            Team& target_team = team_list[team_index[target_name]];
            ProblemStatus& ps = target_team.problems[target_problem];
            int team_initial_rank = initial_rank[target_name];
            
            if (ps.has_frozen_ac) {
                ps.is_solved = true;
                ps.solve_time = ps.frozen_ac_time;
                ps.wrong_attempts += ps.frozen_wrong;
            } else {
                ps.wrong_attempts += ps.frozen_wrong;
            }
            
            ps.is_frozen = false;
            ps.frozen_submissions = 0;
            ps.frozen_wrong = 0;
            ps.has_frozen_ac = false;
            target_team.frozen_count--;
            
            // Remove team from list if no more frozen problems
            if (target_team.frozen_count == 0) {
                teams_with_frozen.erase(teams_with_frozen.begin() + target_idx);
            }
            
            if (ps.is_solved) {
                // Use quick update instead of full flush
                updateTeamRank(team_index[target_name]);
                int new_rank = target_team.current_rank;
                
                if (new_rank < team_initial_rank) {
                    string displaced = rank_to_team[new_rank];
                    if (!displaced.empty() && displaced != target_name) {
                        auto [solved, penalty] = getDisplayStats(target_team);
                        output << target_name << " " << displaced << " " 
                               << solved << " " << penalty << "\n";
                    }
                }
            }
        }
        
        output << getScoreboard();
        is_frozen = false;
        return output.str();
    }
    
    string queryRanking(const string& team_name) {
        if (!team_index.count(team_name)) {
            return "[Error]Query ranking failed: cannot find the team.\n";
        }
        
        stringstream output;
        output << "[Info]Complete query ranking.\n";
        
        if (is_frozen) {
            output << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }
        
        output << team_name << " NOW AT RANKING " << team_list[team_index[team_name]].current_rank << "\n";
        return output.str();
    }
    
    string querySubmission(const string& team_name, const string& problem, const string& status) {
        if (!team_index.count(team_name)) {
            return "[Error]Query submission failed: cannot find the team.\n";
        }
        
        Team& team = team_list[team_index[team_name]];
        stringstream output;
        output << "[Info]Complete query submission.\n";
        
        char p = (problem == "ALL") ? 0 : problem[0];
        
        int best_time = -1;
        int best_idx = -1;
        char result_p = 0;
        string result_status;
        
        for (const auto& [key, val] : team.last_submission) {
            auto& [prob, st] = key;
            
            bool prob_match = (p == 0) || (prob == p);
            bool status_match = (status == "ALL") || (st == status);
            
            if (prob_match && status_match) {
                if (val.first > best_time || (val.first == best_time && val.second > best_idx)) {
                    best_time = val.first;
                    best_idx = val.second;
                    result_p = prob;
                    result_status = st;
                }
            }
        }
        
        if (best_time >= 0) {
            output << team_name << " " << result_p << " " << result_status << " " << best_time << "\n";
        } else {
            output << "Cannot find any submission.\n";
        }
        
        return output.str();
    }
    
    string end() {
        return "[Info]Competition ends.\n";
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    ICPCSystem system;
    string line;
    
    while (getline(cin, line)) {
        if (line.empty()) continue;
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            cout << system.addTeam(team_name);
        }
        else if (command == "START") {
            string dummy;
            int duration, problems;
            iss >> dummy >> duration >> dummy >> problems;
            cout << system.startCompetition(duration, problems);
        }
        else if (command == "SUBMIT") {
            string problem, by, team_name, with, status, at_str;
            int time;
            iss >> problem >> by >> team_name >> with >> status >> at_str >> time;
            system.submit(problem, team_name, status, time);
        }
        else if (command == "FLUSH") {
            cout << system.flush();
        }
        else if (command == "FREEZE") {
            cout << system.freeze();
        }
        else if (command == "SCROLL") {
            cout << system.scroll();
        }
        else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            cout << system.queryRanking(team_name);
        }
        else if (command == "QUERY_SUBMISSION") {
            string team_name, where, problem_cond, and_str, status_cond;
            string problem, status;
            iss >> team_name >> where >> problem_cond >> and_str >> status_cond;
            
            size_t eq_pos = problem_cond.find('=');
            problem = problem_cond.substr(eq_pos + 1);
            
            eq_pos = status_cond.find('=');
            status = status_cond.substr(eq_pos + 1);
            
            cout << system.querySubmission(team_name, problem, status);
        }
        else if (command == "END") {
            cout << system.end();
            break;
        }
    }
    
    return 0;
}
