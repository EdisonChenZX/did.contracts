#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <did.ntoken/did.ntoken_db.hpp>
#include <utils.hpp>

#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>


namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("amax.did")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.did")]]

struct aplink_farm {
    name contract           = "aplink.farm"_n;
    uint64_t lease_id       = 4;    //nftone-farm-land
};

NTBL("global") global_t {
    name                        admin;
    name                        ntf_contract;
    aplink_farm                 apl_farm;
    uint64_t last_order_idx     = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(nft_contract)(apl_farm)(last_order_idx) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL order_t {
    uint64_t        id;                 //PK
    name            maker;
    name            vendor_account;
    uint32_t        kyc_level;
    time_point_sec  created_at;

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_maker const { return maker.value ; }

    typedef eosio::multi_index
    < "orders"_n,  order_t,
        indexed_by<"makeridx"_n,     const_mem_fun<order_t, uint64_t, &order_t::by_maker> >
    > order_idx;

    EOSLIB_SERIALIZE( order_t, (id)(maker)(vendor_account)(kyc_level)(created_at) )
};

//Scope: nasset.symbol.id
TBL vender_info_t {
    uint64_t        id;                 //PK
    string          vender_name;
    name            vendor_account;
    uint32_t        kyc_level;
    asset_t         vendor_charge_quant;        //E.g. "1.000000 MUSDT"
    asset_t         user_reward_quant;          //E.g. "10.0000 APL"
    asset_t         user_charge_amount;         //E.g. "1.500000 MUSDT"
    nsymbol         nft_id;
    name            status;
    time_point_sec  created_at;
    time_point_sec  updated_at;

    vender_info_t() {}
    vender_info_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint128_t by_vendor_account_and_kyc_level const { return (uint128_t) vendor_account.value << 64 | | (uint128_t)kyc_level ; }

    typedef eosio::multi_index
    < "venderinfo"_n,  vender_info_t,
        indexed_by<"venderidx"_n, const_mem_fun<vender_info_t, uint64_t, &vender_info_t::by_vendor_account_and_kyc_level> >
    > vender_info_idx;

    EOSLIB_SERIALIZE( vender_info_t, (id)(vender_name)(vendor_account)(kyc_level)
                                     (vendor_charge_quant)(user_reward_quant)(user_charge_amount)
                                     (nft_id)(status)(created_at)(updated_at) )
};

} //namespace amax