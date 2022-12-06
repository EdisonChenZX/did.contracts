
#include <amax.token.hpp>
#include "redpack.hpp"
#include "amax.ntoken/amax.ntoken.hpp"
#include <utils.hpp>
#include <algorithm>
#include <chrono>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

using std::chrono::system_clock;
using namespace wasm;
using namespace amax;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

// transfer out from contract self
#define NFT_TRANSFER(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    CHECKC(digit >= 0 && digit <= 18, err::SYMBOL_MISMATCH, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

void redpack::feetransfer( name from, name to, asset quantity, string memo )
{
    if (from == _self || to != _self) return;

	CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "quantity must be positive" )

    //memo params format:
    //code : id : parent_id : quantity：nft_contract
    auto parts = split( memo, ":" );
    CHECKC( parts.size() == 5, err::INVALID_FORMAT,"Expected format 'code : id : parent_id : count'" );

    auto code = name(parts[0]);
    auto id = to_uint64(parts[1], "id parse uint error");
    auto parent_id = to_uint64(parts[2], "parent_id parse uint error");
    auto nft_quantity = to_int64(parts[3], "quantity parse int error");
    auto nft_contract = name(parts[4]);

    redpack_t redpack(code);
    CHECKC( !_db.get(redpack), err::RED_PACK_EXIST, "code is already exists" );
    
    fee_t fee_info(nft_contract);
    CHECKC( _db.get(fee_info), err::FEE_NOT_FOUND, "fee not found" );
    CHECKC( quantity >= _calc_fee(fee_info.fee, nft_quantity), err::QUANTITY_NOT_ENOUGH , "not enough " );
    
    nsymbol nsym(id, parent_id);
    int64_t supply_nasset = ntoken::get_supply(nft_contract, nsym);
    CHECKC( supply_nasset > 0, err::RECORD_NO_FOUND, "nft not found" );
    
    nasset redpack_quantity(nft_quantity, nsym);

    redpack_t::idx_t redpacks( _self, _self.value );
    redpacks.emplace( _self, [&]( auto& row ) {
        row.code 					    = code;
        row.sender 			            = from;
        row.fee                         = quantity;
        row.status			            = redpack_status::INIT;
        row.total_quantity              = redpack_quantity;
        row.remain_quantity		        = redpack_quantity;
        row.nft_contract		        = nft_contract;
        row.created_at                  = time_point_sec( current_time_point() );
        row.updated_at                  = time_point_sec( current_time_point() );
   });

}

void redpack::ontransfer( const name& from, const name& to, const vector<nasset>& assets, const string& memo  )
{
    if (from == _self || to != _self) return;

	CHECKC( assets.size() == 1, err::TOO_MANY_TYPES, "only one kind of nft can be sent" )
	CHECKC( assets.at(0).amount > 0, err::NOT_POSITIVE, "quantity must be positive" )
    //memo params format:
    //${pwhash} : code
    auto parts = split( memo, ":" );
    CHECKC( parts.size() == 2, err::INVALID_FORMAT, "Expected format 'pwhash : code'" );
   
    auto pwhash = string(parts[0]);
    auto code = name(parts[1]);

    redpack_t redpack(code);
    CHECKC( _db.get(redpack), err::RECORD_NO_FOUND, "record not found" );
    CHECKC( redpack.sender == from, err::PARAM_ERROR, "redpack sender must be fee sender" );
    CHECKC( assets.at(0) == redpack.total_quantity, err::PARAM_ERROR, "quantity error" );

    redpack.pw_hash                 = pwhash;
    redpack.status			        = redpack_status::CREATED;
    redpack.updated_at              = time_point_sec( current_time_point() );

    _db.set(redpack, _self);
}

void redpack::claimredpack( const name& claimer, const name& code, const string& pwhash )
{
    require_auth( _gstate.tg_admin );

    redpack_t redpack(code);
    CHECKC( _db.get(redpack), err::RECORD_NO_FOUND, "redpack not found" );
    CHECKC( redpack.pw_hash == pwhash, err::PWHASH_INVALID, "incorrect password" );
    CHECKC( redpack.status == redpack_status::CREATED, err::STATUS_ERROR, "redpack status error" );
    
    claim_t::idx_t claims(_self, _self.value);
    auto claims_index = claims.get_index<"unionid"_n>();
    uint128_t sec_index = get_unionid(claimer, code.value);
    auto claims_iter = claims_index.find(sec_index);
    CHECKC( claims_iter == claims_index.end() ,err::NOT_REPEAT_RECEIVE, "Can't repeat to receive" );

    nasset redpack_quantity(1, redpack.total_quantity.symbol);
    vector<nasset> redpack_quants = { redpack_quantity };
    NFT_TRANSFER(redpack.nft_contract, claimer, redpack_quants, string("red pack transfer"));

    redpack.remain_quantity -= redpack_quantity;
    redpack.updated_at = time_point_sec( current_time_point() );
    if(redpack.remain_quantity.amount == 0){
        redpack.status = redpack_status::FINISHED;
    }
    _db.set(redpack, _self);

    auto id = claims.available_primary_key();
    claims.emplace( _self, [&]( auto& row ) {
        row.id                  = id;
        row.red_pack_code 	    = code;
        row.sender              = redpack.sender;
        row.receiver            = claimer;
        row.quantity            = redpack_quantity;
        row.claimed_at		    = time_point_sec( current_time_point() );
   });
}

void redpack::cancel( const name& code )
{
    require_auth( _gstate.tg_admin );
    redpack_t redpack(code);
    CHECKC( _db.get(redpack), err::RECORD_NO_FOUND, "redpack not found" );
    CHECKC( current_time_point() > redpack.created_at + eosio::hours(_gstate.expire_hours), err::NOT_EXPIRED, "expiration date is not reached" );
    if(redpack.status == redpack_status::CREATED){
        fee_t fee_info(redpack.nft_contract);
        CHECKC( _db.get(fee_info), err::FEE_NOT_FOUND, "fee not found" );

        vector<nasset> redpack_quants = { redpack.remain_quantity };
        NFT_TRANSFER(redpack.nft_contract, redpack.sender, redpack_quants, string("red pack cancel transfer"));

        asset cancelamt = _calc_fee(fee_info.fee, redpack.remain_quantity.amount);
        TRANSFER_OUT(fee_info.fee_contract, redpack.sender, cancelamt, string("red pack cancel transfer"));
    }
    _db.del(redpack);
    claim_t::idx_t claims(_self, _self.value);
    auto claims_index = claims.get_index<"packid"_n>();
    auto claims_iter = claims_index.find(code.value);
    while(claims_iter != claims_index.end()){
        claims_index.erase(claims_iter);
        claims_iter = claims_index.find(code.value);
    }
}

void redpack::addfee( const asset& fee, const name& fee_contract, const name& nft_contract)
{
    require_auth( _self );
    CHECKC( fee.amount >= 0, err::FEE_NOT_POSITIVE, "fee must be positive" );

    auto fee_info = fee_t(nft_contract);
    fee_info.fee = fee;
    fee_info.fee_contract = fee_contract;
    _db.set( fee_info, _self );
}

void redpack::delfee( const name& nft_contract )
{
    require_auth( _self );
    auto fee_info = fee_t(nft_contract);
    CHECKC( _db.get(fee_info), err::FEE_NOT_FOUND, "fee not found" );

    _db.del( fee_info );
}

void redpack::setconf(const name& admin, const uint16_t& hours, const bool& enable_did)
{
    require_auth( _self );
    CHECKC( is_account(admin), err::ACCOUNT_INVALID, "account invalid" );
    CHECKC( hours > 0, err::VAILD_TIME_INVALID, "valid time must be positive" );

    _gstate.tg_admin = admin;
    _gstate.expire_hours = hours;
    _gstate.enable_did = enable_did;
}

void redpack::delredpacks(const name& code){
    require_auth( _self );

    redpack_t::idx_t redpacks(_self, _self.value);
    auto redpack_itr = redpacks.find(code.value);
    while(redpack_itr != redpacks.end()){
        redpack_itr =redpacks.erase(redpack_itr);
    }
}

asset redpack::_calc_fee(const asset& fee, const uint64_t count) {
    // calc order quantity value by price
    auto value = multiply<uint64_t>(fee.amount, count);

    return asset(value, fee.symbol);
}



