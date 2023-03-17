
#include <amax.token.hpp>
#include "redpack.hpp"
#include <did.ntoken/did.ntoken.db.hpp>
#include "utils.hpp"
#include <algorithm>
#include <chrono>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

using std::chrono::system_clock;
using namespace wasm;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(bank, to, quantity, memo) \
    { action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    CHECKC(digit >= 0 && digit <= 18, err::SYMBOL_MISMATCH, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

void redpack::setfee(const extended_asset& fee) {
    require_auth( _self );
    CHECKC( fee.quantity.amount>0, err::FEE_NOT_POSITIVE, "fee not positive" );
    
    _gstate2.fee = fee;
}

void redpack::setwhitelist(const extended_symbol& token) {
    require_auth( _self );
    int64_t value = amax::token::get_supply(token.get_contract(), token.get_symbol().code()).amount;
    CHECKC( value > 0, err::SYMBOL_MISMATCH, "symbol mismatch" );
    
    _gstate2.whitelist.insert(token);
}

//issue-in op: transfer tokens to the contract and lock them according to the given plan
void redpack::on_token_transfer( const name& from, const name& to, const asset& quantity, const string& memo)
{
    _token_transfer( from, to, quantity, memo );
}

void redpack::on_mtoken_transfer( const name& from, const name& to, const asset& quantity, const string& memo)
{
    _token_transfer( from, to, quantity, memo );
}

void redpack::_token_transfer( const name& from, const name& to, const asset& quantity, const string& memo )
{
    if (from == _self || to != _self) return;

	CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "quantity must be positive" )

    //memo params format:
    //${pwhash} : count : type : code
    //asset:contract
    auto parts = split(memo, ":");
    if (parts.size() == 4) {
        
        name receiver_contract = get_first_receiver();
        extended_symbol extended_quantity_symbol = extended_symbol(quantity.symbol, receiver_contract); 
        if (!_gstate2.whitelist.count(extended_quantity_symbol)) {
            tokenlist_t::idx_t tokenlist_tbl(_self, _self.value);
            auto tokenlist_index = tokenlist_tbl.get_index<"symcontract"_n>();
            uint128_t sec_index = get_unionid(receiver_contract, quantity.symbol.raw());
            auto tokenlist_iter = tokenlist_index.find(sec_index);
            CHECKC( tokenlist_iter != tokenlist_index.end(), err::NON_RENEWAL, "non-renewal" );
            CHECKC( tokenlist_iter->expired_time > time_point_sec(current_time_point()), err::NON_RENEWAL, "non-renewal" );
       }

        auto code = name(parts[3]);
        redpack_t redpack(code);
        CHECKC(!_db.get(redpack), err::RED_PACK_EXIST, "code is already exists");
        
        auto count = stoi(string(parts[1]));
        auto type = stoi(string(parts[2]));
        CHECKC((redpack_type)type == redpack_type::RANDOM ||
                   (redpack_type)type == redpack_type::MEAN ||
                   (redpack_type)type == redpack_type::DID_RANDOM ||
                   (redpack_type)type == redpack_type::DID_MEAN,
               err::TYPE_INVALID, "redpack type invalid");

        if ((redpack_type)type == redpack_type::DID_RANDOM || (redpack_type)type == redpack_type::DID_MEAN)
            CHECKC(_gstate.did_required, err::UNDER_MAINTENANCE, "did redpack is under maintenance");

        CHECKC((quantity / count).amount >= MIN_SINGLE_REDPACK, err::QUANTITY_NOT_ENOUGH, "not enough ");

        redpack_t::idx_t redpacks(_self, _self.value);
        redpacks.emplace(_self, [&](auto &row){
            row.code 					    = code;
            row.sender 			            = from;
            row.pw_hash                     = string( parts[0] ) + ":" + get_first_receiver().to_string();
            row.total_quantity              = quantity;
            row.receiver_count		        = count;
            row.remain_quantity		        = quantity;
            row.remain_count	            = count;
            row.status			            = redpack_status::CREATED;
            row.type			            = type;
            row.created_at                  = time_point_sec( current_time_point() );
            row.updated_at                  = time_point_sec( current_time_point() ); 
        });
    } else if (parts.size() == 2) {
        name receiver_contract = get_first_receiver();
        extended_asset extended_quantity = extended_asset(quantity, receiver_contract); 
        CHECKC(extended_quantity >= _gstate2.fee, err::QUANTITY_NOT_ENOUGH, "quantity not enough ");

        symbol redpcak_symbol= symbol_from_string(parts[0]);
        name contract = name(parts[1]);
        int64_t value = amax::token::get_supply(contract, redpcak_symbol.code()).amount;
        CHECKC( value > 0, err::SYMBOL_MISMATCH, "symbol mismatch" );
        
        extended_symbol token_symbol = extended_symbol(redpcak_symbol, contract);
        CHECKC( !_gstate2.whitelist.count(token_symbol), err::IN_THE_WHITELIST, "in the whitelist" );

        tokenlist_t::idx_t tokenlist_tbl(_self, _self.value);
        auto tokenlist_index = tokenlist_tbl.get_index<"symcontract"_n>();
        uint128_t sec_index = get_unionid(contract, redpcak_symbol.raw());
        auto tokenlist_iter = tokenlist_index.find(sec_index);
        bool is_exists = tokenlist_iter != tokenlist_index.end();
        auto tid = is_exists ? tokenlist_iter->id : tokenlist_tbl.available_primary_key();
        tokenlist_t token(tid);
        if (is_exists){
            CHECKC( tokenlist_iter->expired_time < time_point_sec(current_time_point()), err::NOT_EXPIRED, "not expired" );
            token.expired_time  = time_point_sec(current_time_point()) + seconds_per_month;
        }else{
            token.expired_time  = time_point_sec(current_time_point()) + seconds_per_month;
            token.sym           = redpcak_symbol;
            token.contract      = contract;
        }
        _db.set(token, _self);
    } else {
        CHECKC( false, err::INVALID_FORMAT, "invalid format" );
    }
}

void redpack::claimredpack( const name& claimer, const name& code, const string& pwhash )
{
    require_auth(_gstate.admin);

    redpack_t redpack(code);
    CHECKC( _db.get(redpack), err::RECORD_NO_FOUND, "redpack not found" );
    auto pw_hash = split(redpack.pw_hash, ":");
    auto contract_name = name(pw_hash[1]);
    if (contract_name.length() == 0) {
        tokenlist_t::idx_t tokenlist_tbl(_self, _self.value);
        auto tokenlist_index = tokenlist_tbl.get_index<"sym"_n>();
        auto tokenlist_iter = tokenlist_index.find(redpack.total_quantity.symbol.raw());
        CHECKC( tokenlist_iter != tokenlist_index.end(), err::RECORD_NO_FOUND, "token list not found" );
        contract_name = tokenlist_iter->contract;
    } 
    CHECKC( pw_hash[0] == pwhash, err::PWHASH_INVALID, "incorrect password");
    CHECKC( redpack.status == redpack_status::CREATED, err::EXPIRED, "redpack has expired" );

    bool is_auth = false;
    if((redpack_type)redpack.type == redpack_type::DID_RANDOM || (redpack_type)redpack.type == redpack_type::DID_MEAN){
        auto claimer_acnts = amax::account_t::idx_t( _gstate2.did_contract, claimer.value );
        bool is_auth = false;
        for( auto claimer_acnts_iter = claimer_acnts.begin(); claimer_acnts_iter!=claimer_acnts.end(); claimer_acnts_iter++ ){
            if(claimer_acnts_iter->balance.amount > 0){
                is_auth = true;
                break;
            }
        }
        CHECKC( is_auth, err::DID_NOT_AUTH, "did is not authenticated" );
    }

    claim_t::idx_t claims(_self, _self.value);
    auto claims_index = claims.get_index<"unionid"_n>();
    uint128_t sec_index = get_unionid(claimer, code.value);
    auto claims_iter = claims_index.find(sec_index);
    CHECKC( claims_iter == claims_index.end() ,err::NOT_REPEAT_RECEIVE, "Can't repeat to receive" );

    asset redpack_quantity;
    switch((redpack_type)redpack.type){
        case redpack_type::RANDOM:
        case redpack_type::DID_RANDOM:
            redpack_quantity = _calc_red_amt(redpack);
            break;

        case redpack_type::MEAN:
        case redpack_type::DID_MEAN:
            redpack_quantity = redpack.remain_count == 1 ? redpack.remain_quantity : redpack.total_quantity/redpack.receiver_count;
            break;
    }
    TRANSFER_OUT(contract_name, claimer, redpack_quantity, string("red pack transfer"));

    redpack.remain_count--;
    redpack.remain_quantity-=redpack_quantity;
    redpack.updated_at = time_point_sec( current_time_point() );
    if(redpack.remain_count == 0){
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
        auto pw_hash = split(redpack.pw_hash, ":");
        auto contract = pw_hash[1];
        if (contract.size() == 0) {
            tokenlist_t::idx_t tokenlist_tbl(_self, _self.value);
            auto tokenlist_index = tokenlist_tbl.get_index<"sym"_n>();
            auto tokenlist_iter = tokenlist_index.find(redpack.total_quantity.symbol.raw());
            CHECKC( tokenlist_iter != tokenlist_index.end(), err::RECORD_NO_FOUND, "token list not found" );
            TRANSFER_OUT(tokenlist_iter->contract, redpack.sender, redpack.remain_quantity, string("red pack cancel transfer"));
        } else {
            auto contract_name = name(pw_hash[1]);
            TRANSFER_OUT(contract_name, redpack.sender, redpack.remain_quantity, string("red pack cancel transfer"));
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
    CHECKC(count > 0, err::NONE_DELETED, "delete invalid");
}

void redpack::delredpacks(const name& code){
    require_auth( _self );

    redpack_t::idx_t redpacks(_self, _self.value);
    auto redpack_itr = redpacks.find(code.value);
    while(redpack_itr != redpacks.end()){
        redpack_itr =redpacks.erase(redpack_itr);
    }
}

// asset redpack::_calc_fee(const asset& fee, const uint64_t count) {
//     // calc order quantity value by price
//     auto value = multiply<uint64_t>(fee.amount, count);

//     return asset(value, fee.symbol);
// }

asset redpack::_calc_red_amt(const redpack_t& redpack) {
    // calc order quantity value by price
    if ( redpack.remain_count == 1 ) {
        return redpack.remain_quantity;
    } else {
        uint64_t quantity = redpack.remain_quantity.amount / redpack.remain_count * 2;
        uint8_t precision = 0;
        if (redpack.remain_quantity.symbol.precision() <= 2)
            precision = 0;
        else
            precision = redpack.remain_quantity.symbol.precision() - 2;

        return asset(rand(asset(quantity, redpack.remain_quantity.symbol), precision), redpack.remain_quantity.symbol);
    }
}

uint64_t redpack::rand(asset max_quantity,  uint16_t min_unit) {
    auto mixedBlock = tapos_block_prefix() * tapos_block_num();
    const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
    auto hash = sha256( (char *)mixedChar, sizeof(mixedChar));
    int64_t min_unit_throot = power10(min_unit);

    auto r1 = (uint64_t)hash.data()[0];
    float rand = 1/min_unit_throot+r1 % 100 / 100.00;
    int64_t round_throot = power10(max_quantity.symbol.precision() - min_unit);

    uint64_t rand_value = (uint64_t)(max_quantity.amount * rand) / round_throot * round_throot;
    uint64_t min_value = get_precision(max_quantity) / min_unit_throot;
    return rand_value < min_value ? min_value : rand_value;
    

}

