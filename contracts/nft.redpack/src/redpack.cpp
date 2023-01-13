
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
    CHECKC( code.length() != 0, err::PARAM_ERROR, "code cannot be empty" );

    auto id = to_uint64(parts[1], "id parse uint error");
    auto parent_id = to_uint64(parts[2], "parent_id parse uint error");

    auto nft_quantity = to_int64(parts[3], "quantity parse int error");
    CHECKC( nft_quantity > 0, err::PARAM_ERROR, "nft quantity must be greater than zero" );

    auto nft_contract = name(parts[4]);

    redpack_t redpack(code);
    CHECKC( !_db.get(redpack), err::RED_PACK_EXIST, "code is already exists" );
    
    fee_t fee_info(nft_contract);
    CHECKC( _db.get(fee_info), err::FEE_NOT_FOUND, "fee not found" );
    CHECKC( quantity >= _calc_fee(fee_info.fee, nft_quantity), err::QUANTITY_NOT_ENOUGH , "not enough " );
    
    nsymbol nsym(id, parent_id);
    nasset balance_nasset = ntoken::get_balance(nft_contract, from, nsym);
    CHECKC( balance_nasset.amount >= nft_quantity, err::QUANTITY_NOT_ENOUGH, "nft balance not enough" );
    
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
    auto nft_contract = get_first_receiver();

	CHECKC( assets.size() == 1, err::TOO_MANY_TYPES, "only one kind of nft can be sent" )
    nasset quantity = assets.at(0);
	CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "quantity must be positive" )
    
    //memo params format:
    //:${pwhash} : code
    auto params = split( memo, ":" );
    CHECKC( params.size() == 2, err::INVALID_FORMAT, "Expected format 'pwhash : code'" );

    auto pwhash = string(params[0]);
    CHECKC( pwhash.size() != 0, err::PARAM_ERROR, "pwhash cannot be empty" );
    auto code = name(params[1]);
    CHECKC( code.length() != 0, err::PARAM_ERROR, "code cannot be empty" );

    redpack_t redpack(code);
    bool is_exists = _db.get(redpack);

    if(is_exists){
        CHECKC( redpack.sender == from, err::PARAM_ERROR, "redpack sender must be fee sender" );
        CHECKC( redpack.nft_contract == nft_contract, err::PARAM_ERROR, "nft contract error" );
        CHECKC( quantity == redpack.total_quantity, err::PARAM_ERROR, "quantity error" );
        CHECKC( redpack.status == redpack_status::INIT, err::PARAM_ERROR, "status error" );

        redpack.pw_hash                 = pwhash;
        redpack.status			        = redpack_status::CREATED;
        redpack.updated_at              = time_point_sec( current_time_point() );
        _db.set(redpack, _self);
    }else{
        fee_t fee_info(nft_contract);
        CHECKC( (_db.get(fee_info) && fee_info.fee.amount == 0), err::FEE_NO_PAID, "service charge not paid" );
        
        redpack_t::idx_t redpacks( _self, _self.value );
        redpacks.emplace( _self, [&]( auto& row ) {
            row.code 					    = code;
            row.sender 			            = from;
            row.pw_hash                     = pwhash;
            row.fee                         = fee_info.fee;
            row.status			            = redpack_status::CREATED;
            row.total_quantity              = quantity;
            row.remain_quantity		        = quantity;
            row.nft_contract		        = nft_contract;
            row.created_at                  = time_point_sec( current_time_point() );
            row.updated_at                  = time_point_sec( current_time_point() );
        });
    }
}

void redpack::claimredpack( const name& claimer, const name& code, const string& pwhash )
{
    require_auth( _gstate.admin );

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
    redpack_t redpack(code);
    CHECKC( _db.get(redpack), err::RECORD_NO_FOUND, "redpack not found" );
    CHECKC( current_time_point() > redpack.created_at + eosio::hours(_gstate.expire_hours), err::NOT_EXPIRED, "expiration date is not reached" );
    if(redpack.status == redpack_status::CREATED){
        fee_t fee_info(redpack.nft_contract);
        CHECKC( _db.get(fee_info), err::FEE_NOT_FOUND, "fee not found" );

        vector<nasset> redpack_quants = { redpack.remain_quantity };
        NFT_TRANSFER(redpack.nft_contract, redpack.sender, redpack_quants, string("red pack cancel transfer"));
        
        if(redpack.fee.amount > 0){
            asset cancelamt = redpack.fee / redpack.total_quantity.amount * redpack.remain_quantity.amount;
            TRANSFER_OUT(fee_info.fee_contract, redpack.sender, cancelamt, string("red pack cancel transfer"));
        }
    }
    _db.del(redpack);
}

void redpack::delclaims( const uint64_t& max_rows )
{    
    set<name> is_not_exist;

    claim_t::idx_t claim_idx(_self, _self.value);
    auto claim_itr = claim_idx.begin();

    size_t count = 0;
    for (; count < max_rows && claim_itr != claim_idx.end(); ) {
        bool redpack_not_existed = is_not_exist.count(claim_itr->red_pack_code) > 0 ? true : false;
        if (!redpack_not_existed){
            redpack_t redpack(claim_itr->red_pack_code);
            redpack_not_existed = !_db.get(redpack);
           
            if (redpack_not_existed){
                claim_itr = claim_idx.erase(claim_itr);
                is_not_exist.insert(claim_itr->red_pack_code);
                count++;
            } else {
                break;
            }
        } else {
            claim_itr = claim_idx.erase(claim_itr);
            count++;
        }
    }
    CHECKC(count > 0, err::DEL_INVALID, "delete invalid");
}

void redpack::addfee( const asset& fee, const name& fee_contract, const name& nft_contract)
{
    require_auth( _self );
    CHECKC( fee.amount >= 0, err::FEE_NOT_POSITIVE, "fee must be positive" );
    CHECKC( is_account(nft_contract), err::ACCOUNT_INVALID, "account invalid" );

    asset supply_asset = amax::token::get_supply(fee_contract, fee.symbol.code());
    CHECKC( supply_asset.amount > 0, err::RECORD_NO_FOUND, "token not found" );
    CHECKC( fee.symbol.precision() == supply_asset.symbol.precision(), err::PRECISION_MISMATCH, "precision mismatch" );
 
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

void redpack::setconf(const name& admin, const uint16_t& hours)
{
    require_auth( _self );
    CHECKC( is_account(admin), err::ACCOUNT_INVALID, "account invalid" );
    CHECKC( hours > 0, err::VAILD_TIME_INVALID, "valid time must be positive" );

    _gstate.admin = admin;
    _gstate.expire_hours = hours;
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



