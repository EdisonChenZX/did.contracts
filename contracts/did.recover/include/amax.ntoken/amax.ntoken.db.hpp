#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace ntoken {

using namespace std;
using namespace eosio;


struct nsymbol {
    uint32_t id;
    uint32_t parent_id;

    nsymbol() {}
    nsymbol(const uint32_t& i): id(i),parent_id(0) {}
    nsymbol(const uint32_t& i, const uint32_t& pid): id(i), parent_id(pid) {}

    friend bool operator==(const nsymbol&, const nsymbol&);
    friend bool operator<(const nsymbol&, const nsymbol&);

    bool is_valid()const { return( id > parent_id ); }
    uint64_t raw()const { return( (uint64_t) parent_id << 32 | id ); }

    EOSLIB_SERIALIZE( nsymbol, (id)(parent_id) )
};

bool operator==(const nsymbol& symb1, const nsymbol& symb2) {
    return( symb1.id == symb2.id && symb1.parent_id == symb2.parent_id );
}

bool operator<(const nsymbol& symb1, const nsymbol& symb2) {
    return( symb1.id < symb2.id || symb1.parent_id < symb2.parent_id );
}

struct nasset {
    int64_t         amount;
    nsymbol         symbol;

    nasset() {}
    nasset(const uint32_t& id): symbol(id), amount(0) {}
    nasset(const uint32_t& id, const uint32_t& pid): symbol(id, pid), amount(0) {}
    nasset(const uint32_t& id, const uint32_t& pid, const int64_t& am): symbol(id, pid), amount(am) {}
    nasset(const int64_t& amt, const nsymbol& symb): amount(amt), symbol(symb) {}

    friend bool operator==(const nasset& n1, const nasset& n2) {
        return( n1.symbol == n2.symbol && n1.amount == n2.amount );
    }

    nasset& operator+=(const nasset& quantity) {
        check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
        this->amount += quantity.amount; return *this;
    }
    nasset& operator-=(const nasset& quantity) {
        check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
        this->amount -= quantity.amount; return *this;
    }

    bool is_valid()const { return symbol.is_valid(); }

    EOSLIB_SERIALIZE( nasset, (amount)(symbol) )
};

///Scope: owner's account
struct account_t {
    nasset      balance;
    bool        paused = false;   //if true, it can no longer be transferred

    account_t() {}
    account_t(const nasset& asset): balance(asset) {}

    uint64_t primary_key()const { return balance.symbol.raw(); }

    EOSLIB_SERIALIZE(account_t, (balance)(paused) )

    typedef eosio::multi_index< "accounts"_n, account_t > idx_t;
};


} //namespace amax