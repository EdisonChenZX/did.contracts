#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>


#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>


namespace amax {

using namespace std;
using namespace eosio;

namespace ActionType {
    static constexpr eosio::name    CREATEORDER    { "createorder"_n  }; //create a order
    static constexpr eosio::name    VERIFYDID      { "verifydid"_n    }; 
    static constexpr eosio::name    VERIFYVOTE     { "verifyvote"_n  }; 
    static constexpr eosio::name    ASSERTRECAST   { "assetrecast"_n  }; 
    static constexpr eosio::name    DELORDER       { "delorder"_n  }; 
}

namespace OrderStatus {
    static constexpr eosio::name PENDING     { "pending"_n   };
    static constexpr eosio::name FINISHED    { "finished"_n  };
    static constexpr eosio::name FAILED      { "failed"_n   };
    static constexpr eosio::name CANCELLED   { "cancelled"_n };
}

static constexpr uint64_t seconds_per_day                   = 24 * 3600;
static constexpr uint64_t order_expiry_duration             = seconds_per_day;
static constexpr uint64_t manual_order_expiry_duration      = 3 * seconds_per_day;

static constexpr name DID_CONTRACTT = "did.ntoken"_n;
static constexpr uint32_t DID_SYMBOL_ID = 1000001;
// static constexpr uint32_t DID_SYMBOL_ID = 1;
static constexpr uint32_t MAX_TITLE_SIZE        = 256;

#define TBL struct [[eosio::table, eosio::contract("did.recover")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("did.recover")]]

NTBL("global") global_t {
    extended_asset              fee_info;
    name                        fee_collector;
    bool                        enabled = true;
    uint64_t                    last_order_id;

    EOSLIB_SERIALIZE( global_t, (fee_info)(fee_collector)(enabled)(last_order_id))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//Scope: _self 
TBL recover_order_t {             
    uint64_t                    id                  = 0;                    //PK        
    name                        lost_account;       // PK
    name                        owner;              // UK
    name                        status                      = OrderStatus::PENDING;
    name                        did_certification_status    = OrderStatus::PENDING;
    // name                        vote_certification_status   = OrderStatus::PENDING;
    name                        asset_recast_status         = OrderStatus::PENDING;
    time_point_sec              created_at;
    time_point_sec              did_expired_at;
    time_point_sec              updated_at;

    recover_order_t() {}
    recover_order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id ; }
    uint64_t by_account() const { return lost_account.value; }
    uint64_t by_owner() const { return owner.value; }

    typedef eosio::multi_index
    < "recorders"_n,  recover_order_t,
        indexed_by<"accountidx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_account> >,
        indexed_by<"owneridx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_owner> >
    > idx_t;


    EOSLIB_SERIALIZE( recover_order_t, (id)(lost_account)(owner)(status)(did_certification_status)
                    (asset_recast_status)(created_at)(did_expired_at)(updated_at) )
};

//Scope: _self
TBL auth_t {
    name                        auth;              //PK
    set<name>                   actions;              //set of action types

    auth_t() {}
    auth_t(const name& i): auth(i) {}

    uint64_t primary_key()const { return auth.value; }

    typedef eosio::multi_index< "auths"_n,  auth_t > idx_t;

    EOSLIB_SERIALIZE( auth_t, (auth)(actions) )
};
} //namespace amax
