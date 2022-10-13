#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <utils.hpp>

#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>
#include <did.ntoken/did.ntoken.hpp>


namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("amax.did")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.did")]]

struct aplink_farm {
    name contract           = "aplink.farm"_n;
    uint64_t lease_id       = 5;    //nftone-farm-land
};

NTBL("global") global_t {
    name                        admin;
    name                        nft_contract;
    name                        fee_collector;
    aplink_farm                 apl_farm;
    uint64_t                    last_order_idx = 0;
    uint64_t                    last_vendor_id = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(nft_contract)(fee_collector)(apl_farm)(last_order_idx)(last_vendor_id) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL order_t {
    uint64_t        id;                 //PK
    name            maker;
    name            vendor_account;
    uint32_t        kyc_level;
    string          secret_md5;
    time_point_sec  created_at;

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_maker() const { return maker.value ; }

    typedef eosio::multi_index
    < "orders"_n,  order_t,
        indexed_by<"makeridx"_n,     const_mem_fun<order_t, uint64_t, &order_t::by_maker> >
    > order_idx;

    EOSLIB_SERIALIZE( order_t, (id)(maker)(vendor_account)(kyc_level)(secret_md5)(created_at) )
};

//Scope: nasset.symbol.id
TBL vendor_info_t {
    uint64_t        id;                 //PK
    string          vendor_name;
    name            vendor_account;
    uint32_t        kyc_level;
    asset           user_reward_quant;          //E.g. "10.0000 APL"
    asset           user_charge_amount;         //E.g. "1.500000 MUSDT"
    nsymbol         nft_id;
    name            status;
    time_point_sec  created_at;
    time_point_sec  updated_at;

    vendor_info_t() {}
    vendor_info_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint128_t by_vendor_account_and_kyc_level() const { return ((uint128_t) vendor_account.value << 64) + kyc_level ; }

    typedef eosio::multi_index
    < "vendorinfo"_n,  vendor_info_t,
        indexed_by<"vendoridx"_n, const_mem_fun<vendor_info_t, uint128_t, &vendor_info_t::by_vendor_account_and_kyc_level> >
    > idx_t;

    EOSLIB_SERIALIZE( vendor_info_t, (id)(vendor_name)(vendor_account)(kyc_level)
                                     (user_reward_quant)(user_charge_amount)
                                     (nft_id)(status)(created_at)(updated_at) )
};

} //namespace amax