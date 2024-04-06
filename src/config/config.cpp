#include "config/config.h"

namespace bench{
    std::string Configuration::DebugString(){
        std::string debug_string = fmt::format("CfgId: {} StartTime:{} Number of fragment: {}\n", cfg_id, start_time_in_seconds, fragments.size());
        for (int i = 0; i < fragments.size(); i++){
            debug_string += fmt::format("frag[{}]: {}-{}-{}-{}\n",
                                    i, 
                                    fragments[i]->range.key_start,
                                    fragments[i]->range.key_end,
                                    fragments[i]->server_id,
                                    fragments[i]->dbid);
        }
        return debug_string;
    }
}