#include <did.recover/did.recover.hpp>
#include <amax.token/amax.token.hpp>
#include <amax.ntoken/amax.ntoken.db.hpp>

static constexpr eosio::name active_permission{"active"_n};

namespace amax {
   using namespace std;

   #define CHECKC(exp, code, msg) \
      { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }

   void did_recover::init( const extended_asset& fee_info, const name& fee_collector) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");
      
      _gstate.fee_info           = fee_info;
      _gstate.fee_collector      = fee_collector;
   }

   void did_recover::on_amax_transfer(const name& from, const name& to, const asset& quant, const string& memo) {
      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      vector<string_view> params = split(memo, ":");
      auto param_size            = params.size();
      CHECKC( param_size == 1, err::MEMO_FORMAT_ERROR, "memo format incorrect" )

      name lost_account = name(params[0]);
      CHECKC( is_account(lost_account), err::ACCOUNT_INVALID,"ACCOUNT INVALID")

      name contract = get_first_receiver();
      CHECKC( contract == _gstate.fee_info.contract , err::PARAM_ERROR, "asset contract must be " + _gstate.fee_info.contract.to_string())
      CHECKC( quant == _gstate.fee_info.quantity , err::PARAM_ERROR, "The handling fee is not met")

      _create_order(from,lost_account);

      if (  _gstate.fee_collector != _self )
         TRANSFER_X( _gstate.fee_info.contract, _gstate.fee_collector, quant, "did recover fee" )
   }

   // void did_recover::cancelorder( const name& account ) {

   //    auto order = recover_order_t( account );
   //    CHECKC( _db.get( order ), err::RECORD_NOT_FOUND, "order not found." )
   //    require_auth( order.owner );
   //    CHECKC( order.status != OrderStatus::FINISHED , err::STATUS_ERROR,"This status cannot be cancelled" )
      
   //    _db.del(order);
   // }

   void did_recover::verifydid( const name& submitter, const uint64_t& order_id, const bool& passed) {
       
      _check_action_auth(submitter, ActionType::VERIFYDID);

      auto order = recover_order_t( order_id );
      CHECKC( _db.get( order ), err::RECORD_NOT_FOUND, "order not found." )
      CHECKC( order.status == OrderStatus::PENDING, err::STATUS_ERROR,"This status cannot be pending" )
      CHECKC( order.did_certification_status != OrderStatus::FINISHED, err::STATUS_ERROR,"DID verification completed" )
      // CHECKC( order.did_expired_at > current_time_point(), err::TIME_EXPIRED,"order already time expired")

      order.did_certification_status = passed ? OrderStatus::FINISHED : OrderStatus::FAILED ;
      order.updated_at = current_time_point();
      

      _on_audit_log( order_id, submitter, order.owner, order.lost_account,ActionType::VERIFYDID,passed);

      if ( passed ){
         _db.set(order);
      }else {
         // order.status = OrderStatus::FAILED;
         // _db.set(order);
         _db.del(order);
      }
      
   }

   // void did_recover::verifyvote( const name& submitter, const name& account, const bool& passed) {
      
   //    _check_action_auth(submitter, ActionType::VERIFYVOTE);

   //    auto order = recover_order_t( account );
   //    CHECKC( _db.get( order ), err::RECORD_NOT_FOUND, "order not found." )
   //    CHECKC( order.status == OrderStatus::PENDING, err::STATUS_ERROR,"This status cannot be pending" )
   //    CHECKC( order.did_certification_status == OrderStatus::FINISHED, err::STATUS_ERROR,"DID verification not completed" )
   //    CHECKC( order.vote_certification_status == OrderStatus::PENDING, err::STATUS_ERROR,"This vote status cannot be pending" )

   //    order.vote_certification_status = passed ? OrderStatus::FINISHED : OrderStatus::FAILED ;
      
   //    order.updated_at = current_time_point();
      
   //    _db.set(order);
   // }

   void did_recover::assetrecast( const name& submitter, const uint64_t& order_id, const bool& passed) {
      
      _check_action_auth(submitter, ActionType::ASSERTRECAST);

      auto order = recover_order_t( order_id );
      CHECKC( _db.get( order ), err::RECORD_NOT_FOUND, "order not found." )
      CHECKC( order.status == OrderStatus::PENDING, err::STATUS_ERROR,"order status must be pending." )
      CHECKC( order.did_certification_status == OrderStatus::FINISHED, err::STATUS_ERROR,"DID verification not completed" )
      // CHECKC( order.vote_certification_status == OrderStatus::FINISHED, err::STATUS_ERROR,"Vote verification not completed" )
      CHECKC( order.asset_recast_status != OrderStatus::FINISHED, err::STATUS_ERROR,"asset recast completed" )

      order.asset_recast_status = passed ? OrderStatus::FINISHED : OrderStatus::FAILED ;
      order.updated_at = current_time_point();
      

      _on_audit_log( order_id, submitter, order.owner, order.lost_account,ActionType::VERIFYDID,passed);

      if ( passed ){
         order.status = OrderStatus::FINISHED;
         _db.set(order);
      }else {
         // order.status = OrderStatus::FAILED;
         // _db.set(order);
         _db.del(order);
      }

     
   }

   void did_recover::delorder( const name& submitter, const uint64_t& order_id) {

      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )

      recover_order_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 

      if ( order_ptr -> did_certification_status == OrderStatus::PENDING 
         || order_ptr -> did_certification_status == OrderStatus::FAILED  ){

         CHECKC(order_ptr->did_expired_at < current_time_point(), err::STATUS_ERROR, "order has not expired")

         _on_audit_log( order_id, submitter, order_ptr->owner, order_ptr->lost_account,ActionType::DELORDER,false);
         orders.erase(order_ptr);   
      }else {
         CHECKC( false, err::RECORD_NOT_FOUND, "Order cannot be deleted"); 
      }
   }

   void did_recover::setauth( const name& auth, const set<name>& actions ) {
      require_auth(_self);      
      CHECKC(is_account(auth), err::PARAM_ERROR,  "account invalid: " + auth.to_string());

      auth_t::idx_t auths(_self, _self.value);
      auto auth_ptr = auths.find(auth.value);

      if( auth_ptr != auths.end() ) {
         auths.modify(*auth_ptr, _self, [&]( auto& row ) {
            row.actions      = actions;
         });   
      } else {
         auths.emplace(_self, [&]( auto& row ) {
            row.auth      = auth;
            row.actions      = actions;
         });
      }
   }

   void did_recover::delauth(  const name& account ) {
      require_auth(_self);    

      auth_t::idx_t auths(_self, _self.value);
      auto auth_ptr     = auths.find(account.value);

      CHECKC( auth_ptr != auths.end(), err::RECORD_EXISTING, "auth not exist. ");
      auths.erase(auth_ptr);
   }

   void did_recover::_check_action_auth(const name& auth, const name& action_type) {
      CHECKC(has_auth(auth),  err::NO_AUTH, "no auth for operate: " + auth.to_string());      

      auto auth_itr     = auth_t(auth);
      CHECKC( _db.get(auth_itr), err::RECORD_NOT_FOUND, "did_recover auth not exist. ");
      CHECKC( auth_itr.actions.count(action_type), err::NO_AUTH, "did_recover no action for " + auth.to_string());
   }

   void did_recover::_create_order(const name& owner, const name& account) {
      
      CHECKC( owner != account, err::PARAM_ERROR, "Unable to submit one's own account")

      auto accounts = ntoken::account_t::idx_t( DID_CONTRACTT, account.value );
      auto did_itr = accounts.find( DID_SYMBOL_ID );
      CHECKC( did_itr !=  accounts.end() ,err::ACCOUNT_INVALID , "Non DID users")

      recover_order_t::idx_t orders( _self, _self.value );
      auto account_idx   = orders.get_index<"accountidx"_n>();
      auto find_itr = account_idx.find(account.value);
      
      CHECKC( find_itr == account_idx.end() , err::RECORD_NOT_FOUND, "The account has been registered and lost its private key" )

      orders.emplace( _self, [&]( auto& row ) {
            row.id               = ++ _gstate.last_order_id;
            row.lost_account     = account;
            row.owner            = owner;
            row.did_expired_at   = current_time_point() + eosio::seconds(order_expiry_duration);
            row.created_at       = current_time_point();
      });
   }

   void did_recover::auditlog(const uint64_t& order_id,
                  const name& submitter, 
                  const name& owner,
                  const name& recover_name,
                  const name& type, 
                  const bool& passed,
                  const time_point&  created_at) {
      require_auth(get_self());
      require_recipient(owner);

   }

   void did_recover::_on_audit_log(const uint64_t& order_id,
                  const name& submitter, 
                  const name& owner,
                  const name& recover_name,
                  const name& type, 
                  const bool& passed) {
            did_recover::auditlog_action act{ _self, { {_self, active_permission} } };
            act.send(order_id, submitter, owner, recover_name, type, passed, current_time_point());
   }
}
