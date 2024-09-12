#pragma once

#include <nano/store/rep_weight.hpp>

namespace nano::store::rocksdb
{
class component;
}
namespace nano::store::rocksdb
{
class rep_weight : public nano::store::rep_weight
{
private:
	nano::store::rocksdb::component & store;

public:
	explicit rep_weight (nano::store::rocksdb::component & store_a);
	uint64_t count (store::transaction const & txn_a) override;
	nano::uint128_t get (store::transaction const & txn_a, nano::account const & representative_a) override;
	void put (store::write_transaction const & txn_a, nano::account const & representative_a, nano::uint128_t const & weight_a) override;
	void del (store::write_transaction const &, nano::account const & representative_a) override;
	store::iterator<nano::account, nano::uint128_union> begin (store::transaction const & txn_a, nano::account const & representative_a) const override;
	store::iterator<nano::account, nano::uint128_union> begin (store::transaction const & txn_a) const override;
	store::iterator<nano::account, nano::uint128_union> end () const override;
	void for_each_par (std::function<void (store::read_transaction const &, store::iterator<nano::account, nano::uint128_union>, store::iterator<nano::account, nano::uint128_union>)> const & action_a) const override;
};
}
