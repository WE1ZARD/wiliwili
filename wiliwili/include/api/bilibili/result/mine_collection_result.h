//
// Created by fang on 2022/7/28.
//

#pragma once

#include "nlohmann/json.hpp"
#include "bilibili/result/user_result.h"


namespace bilibili {

    class CollectionResult {
    public:
        unsigned int id;
        std::string cover;
        std::string title;
        unsigned int ctime;
        unsigned int media_count;
        unsigned int attr;
        UserSimpleResult upper;
    };
    inline void from_json(const nlohmann::json& nlohmann_json_j, CollectionResult& nlohmann_json_t) {
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, cover, id, title, ctime, media_count, attr, upper));
    }

    typedef std::vector<CollectionResult> CollectionListResult;

    class CollectionListResultWrapper {
    public:
        CollectionListResult list;
        bool has_more;
        int count;
        UserSimpleResult upper;
        unsigned int index;
    };
    inline void from_json(const nlohmann::json& nlohmann_json_j, CollectionListResultWrapper& nlohmann_json_t) {
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, has_more, list, count));
    }

    class VideoCollectionStateResult {
    public:
        unsigned int collect;
        unsigned int danmaku;
        unsigned int play;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VideoCollectionStateResult, collect, danmaku, play);

    class CollectionVideoResult{
    public:
        int id;
        int type;
        std::string title;
        std::string cover;
        std::string intro;
        UserSimpleResult upper;
        VideoCollectionStateResult cnt_info;
        int duration;
        unsigned int pubtime;
        std::string bvid;
    };
    inline void from_json(const nlohmann::json& nlohmann_json_j, CollectionVideoResult& nlohmann_json_t) {
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, id, type, title, intro, cover, upper, cnt_info, duration, pubtime, bvid));
    }

    typedef std::vector<CollectionVideoResult>  CollectionVideoListResult;


    class CollectionVideoListResultWrapper {
    public:
        CollectionVideoListResult medias;
        bool has_more;
        CollectionResult info;
        unsigned int index;
    };
    inline void from_json(const nlohmann::json& nlohmann_json_j, CollectionVideoListResultWrapper& nlohmann_json_t) {
        if(nlohmann_json_j.at("medias").is_array()){
            nlohmann_json_j.at("medias").get_to(nlohmann_json_t.medias);
        }
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, has_more, info));
    }
};