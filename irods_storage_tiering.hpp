

#ifndef IRODS_STORAGE_TIERING_HPP
#define IRODS_STORAGE_TIERING_HPP

#include <list>
#include <boost/any.hpp>
#include <string>

#include "rcMisc.h"
#include "storage_tiering_configuration.hpp"

namespace irods {
    using resource_index_map = std::map<std::string, std::string>;
    class storage_tiering {
        public:
        struct policy {
            static const std::string storage_tiering;
            static const std::string data_movement;
            static const std::string access_time;
        };

        struct schedule {
            static const std::string storage_tiering;
            static const std::string data_movement;
        };

        storage_tiering(
            ruleExecInfo_t*    _rei,
            const std::string& _instance_name);

        void apply_policy_for_tier_group(
            const std::string& _group);

        void migrate_object_to_minimum_restage_tier(
            std::list<boost::any>& _args);

        void schedule_storage_tiering_policy(
            const std::string& _json,
            const std::string& _params);

        private:
        void update_access_time_for_data_object(
            const std::string& _logical_path);

        std::string get_metadata_for_resource(
            const std::string& _meta_attr_name,
            const std::string& _resource_name);

        typedef std::vector<std::pair<std::string, std::string>> metadata_results;
        void get_metadata_for_resource(
            const std::string& _meta_attr_name,
            const std::string& _resource_name,
            metadata_results&  _results);

        resource_index_map
        get_tier_group_resource_ids_and_indices(
            const std::string& _group_name);

        std::string get_leaf_resources_string(
            const std::string& _resource_name);

        bool get_preserve_replicas_for_resc(
            const std::string& _source_resource);

        std::string get_verification_for_resc(
            const std::string& _resource_name);

        std::string get_restage_tier_resource_name(
            const std::string& _resource_name);

        std::string get_data_movement_parameters_for_resc(
            const std::string& _resource_name);

        resource_index_map
        get_resource_map_for_group(
            const std::string& _group);
        
        std::string get_tier_time_for_resc(
            const std::string& _resource_name);
        
        metadata_results get_violating_queries_for_resource(
            const std::string& _resource_name);
        
        uint32_t get_object_limit_for_resource(
            const std::string& _resource_name);

        void queue_data_movement(
            const std::string& _plugin_instance_name,
            const std::string& _object_path,
            const std::string& _source_resource,
            const std::string& _destination_resource,
            const std::string& _verification_type,
            const bool         _preserve_replicas,
            const std::string& _data_movement_params);

        void migrate_violating_data_objects(
            const std::string& _source_resource,
            const std::string& _destination_resource);
        
        // Attributes 
        ruleExecInfo_t*               rei_;
        rsComm_t*                     comm_;
        storage_tiering_configuration config_;


    }; // class storage_tiering
}; // namespace irods

#endif // IRODS_STORAGE_TIERING_HPP
