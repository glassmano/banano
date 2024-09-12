#include <nano/lib/blocks.hpp>
#include <nano/node/active_elections.hpp>
#include <nano/node/election.hpp>
#include <nano/node/node.hpp>
#include <nano/node/scheduler/bucket.hpp>

/*
 * bucket
 */

nano::scheduler::bucket::bucket (nano::uint128_t minimum_balance_a, priority_bucket_config const & config_a, nano::active_elections & active_a, nano::stats & stats_a) :
	config{ config_a },
	minimum_balance{ minimum_balance_a },
	active{ active_a },
	stats{ stats_a }
{
}

nano::scheduler::bucket::~bucket ()
{
}

bool nano::scheduler::bucket::available () const
{
	nano::lock_guard<nano::mutex> lock{ mutex };

	if (queue.empty ())
	{
		return false;
	}
	else
	{
		return election_vacancy (queue.begin ()->time);
	}
}

bool nano::scheduler::bucket::election_vacancy (priority_t candidate) const
{
	debug_assert (!mutex.try_lock ());

	if (elections.size () < config.reserved_elections || elections.size () < config.max_elections)
	{
		return active.vacancy (nano::election_behavior::priority) > 0;
	}
	if (!elections.empty ())
	{
		auto lowest = elections.get<tag_priority> ().begin ()->priority;

		// Compare to equal to drain duplicates
		if (candidate <= lowest)
		{
			// Bound number of reprioritizations
			return elections.size () < config.max_elections * 2;
		};
	}
	return false;
}

bool nano::scheduler::bucket::election_overfill () const
{
	debug_assert (!mutex.try_lock ());

	if (elections.size () < config.reserved_elections)
	{
		return false;
	}
	if (elections.size () < config.max_elections)
	{
		return active.vacancy (nano::election_behavior::priority) < 0;
	}
	return true;
}

bool nano::scheduler::bucket::activate ()
{
	nano::lock_guard<nano::mutex> lock{ mutex };

	if (queue.empty ())
	{
		return false; // Not activated
	}

	block_entry top = *queue.begin ();
	queue.erase (queue.begin ());

	auto block = top.block;
	auto priority = top.time;

	auto erase_callback = [this] (std::shared_ptr<nano::election> election) {
		nano::lock_guard<nano::mutex> lock{ mutex };
		elections.get<tag_root> ().erase (election->qualified_root);
	};

	auto result = active.insert (block, nano::election_behavior::priority, erase_callback);
	if (result.inserted)
	{
		release_assert (result.election);
		elections.get<tag_root> ().insert ({ result.election, result.election->qualified_root, priority });

		stats.inc (nano::stat::type::election_bucket, nano::stat::detail::activate_success);
	}
	else
	{
		stats.inc (nano::stat::type::election_bucket, nano::stat::detail::activate_failed);
	}

	return result.inserted;
}

void nano::scheduler::bucket::update ()
{
	nano::lock_guard<nano::mutex> lock{ mutex };

	if (election_overfill ())
	{
		cancel_lowest_election ();
	}
}

// Returns true if the block was inserted
bool nano::scheduler::bucket::push (uint64_t time, std::shared_ptr<nano::block> block)
{
	nano::lock_guard<nano::mutex> lock{ mutex };

	auto [it, inserted] = queue.insert ({ time, block });
	release_assert (!queue.empty ());
	bool was_last = (it == --queue.end ());
	if (queue.size () > config.max_blocks)
	{
		queue.erase (--queue.end ());
		return inserted && !was_last;
	}
	return inserted;
}

size_t nano::scheduler::bucket::size () const
{
	nano::lock_guard<nano::mutex> lock{ mutex };
	return queue.size ();
}

bool nano::scheduler::bucket::empty () const
{
	nano::lock_guard<nano::mutex> lock{ mutex };
	return queue.empty ();
}

size_t nano::scheduler::bucket::election_count () const
{
	nano::lock_guard<nano::mutex> lock{ mutex };
	return elections.size ();
}

void nano::scheduler::bucket::cancel_lowest_election ()
{
	debug_assert (!mutex.try_lock ());

	if (!elections.empty ())
	{
		elections.get<tag_priority> ().begin ()->election->cancel ();

		stats.inc (nano::stat::type::election_bucket, nano::stat::detail::cancel_lowest);
	}
}

std::deque<std::shared_ptr<nano::block>> nano::scheduler::bucket::blocks () const
{
	nano::lock_guard<nano::mutex> lock{ mutex };

	std::deque<std::shared_ptr<nano::block>> result;
	for (auto const & item : queue)
	{
		result.push_back (item.block);
	}
	return result;
}

void nano::scheduler::bucket::dump () const
{
	for (auto const & item : queue)
	{
		std::cerr << item.time << ' ' << item.block->hash ().to_string () << '\n';
	}
}

/*
 * block_entry
 */

bool nano::scheduler::bucket::block_entry::operator< (block_entry const & other_a) const
{
	return time < other_a.time || (time == other_a.time && block->hash () < other_a.block->hash ());
}

bool nano::scheduler::bucket::block_entry::operator== (block_entry const & other_a) const
{
	return time == other_a.time && block->hash () == other_a.block->hash ();
}

/*
 * priority_bucket_config
 */

nano::error nano::scheduler::priority_bucket_config::serialize (nano::tomlconfig & toml) const
{
	toml.put ("max_blocks", max_blocks, "Maximum number of blocks to sort by priority per bucket. \nType: uint64");
	toml.put ("reserved_elections", reserved_elections, "Number of guaranteed slots per bucket available for election activation. \nType: uint64");
	toml.put ("max_elections", max_elections, "Maximum number of slots per bucket available for election activation if the active election count is below the configured limit. \nType: uint64");

	return toml.get_error ();
}

nano::error nano::scheduler::priority_bucket_config::deserialize (nano::tomlconfig & toml)
{
	toml.get ("max_blocks", max_blocks);
	toml.get ("reserved_elections", reserved_elections);
	toml.get ("max_elections", max_elections);

	return toml.get_error ();
}