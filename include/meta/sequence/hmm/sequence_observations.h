/**
 * @file sequence_observations.h
 * @author Chase Geigle
 *
 * All files in META are dual-licensed under the MIT and NCSA licenses. For more
 * details, consult the file LICENSE.mit and LICENSE.ncsa in the root of the
 * project.
 */

#ifndef META_SEQUENCE_HMM_SEQUENCE_OBS_H_
#define META_SEQUENCE_HMM_SEQUENCE_OBS_H_

#include "meta/sequence/hmm/hmm.h"
#include "meta/stats/multinomial.h"

namespace meta
{
namespace sequence
{
namespace hmm
{

/**
 * A Markov Model observation distribution for HMMs. Each observation is
 * assumed to be a sequence of states. Each *HMM* state is modeled via a
 * separate Markov model.
 */
template <class StateType>
class sequence_observations
{
  public:
    using observation_type = std::vector<StateType>;

    struct markov_model
    {
        template <class Generator>
        markov_model(uint64_t num_states, Generator&& rng,
                     stats::dirichlet<StateType> prior)
            : trans_dists_(num_states, prior)
        {
            for (StateType s_i{0}; s_i < num_states; ++s_i)
            {
                auto rnd = random::bounded_rand(rng, 65536);
                auto val = (rnd / 65536.0) / num_states;
                initial_dist_.increment(s_i, val);

                for (StateType s_j{0}; s_j < num_states; ++s_j)
                {
                    auto rnd = random::bounded_rand(rng, 65536);
                    auto val = (rnd / 65536.0) / num_states;

                    trans_dists_[s_i].increment(s_j, val);
                }
            }
        }

        markov_model(uint64_t num_states, stats::dirichlet<StateType> prior)
            : trans_dists_(num_states, prior)
        {
            // nothing
        }

        const stats::dirichlet<StateType>& prior() const
        {
            return trans_dists_.front().prior();
        }

        stats::multinomial<StateType> initial_dist_;
        std::vector<stats::multinomial<StateType>> trans_dists_;
    };

    /**
     * Initializes each state's Markov model randomly using the provided
     * random number generator.
     */
    template <class Generator>
    sequence_observations(uint64_t num_hmm_states, uint64_t num_markov_states,
                          Generator&& gen,
                          stats::dirichlet<StateType> trans_prior)
    {
        models_.reserve(num_hmm_states);
        for (uint64_t h = 0; h < num_hmm_states; ++h)
            models_.emplace_back(num_markov_states,
                                 std::forward<Generator>(gen), trans_prior);
    }

    /**
     * Default initializes each state's Markov model. This is only useful
     * when setting values manually by using increment().
     */
    sequence_observations(uint64_t num_hmm_states, uint64_t num_markov_states,
                          stats::dirichlet<StateType> trans_prior)
    {
        models_.reserve(num_hmm_states);
        for (uint64_t h = 0; h < num_hmm_states; ++h)
            models_.emplace_back(num_markov_states, trans_prior);
    }

    uint64_t num_states() const
    {
        return models_.size();
    }

    sequence_observations blank() const
    {
        return {models_.size(), models_.front().trans_dists_.size(),
                models_.front().prior()};
    }

    const stats::dirichlet<StateType>& prior() const
    {
        return models_.front().prior();
    }

    double probability(const observation_type& obs, state_id s_i) const
    {
        const auto& model = models_[s_i];

        double log_prob = std::log(model.initial_dist_.probability(obs[0]));
        for (uint64_t t = 1; t < obs.size(); ++t)
        {
            log_prob
                += std::log(model.trans_dists_[obs[t - 1]].probability(obs[t]));
        }
        return std::exp(log_prob);
    }

    void increment(const observation_type& obs, state_id s_i, double amount)
    {
        auto& model = models_[s_i];

        model.initial_dist_.increment(obs[0], amount);
        for (uint64_t t = 1; t < obs.size(); ++t)
        {
            model.trans_dists_[obs[t - 1]].increment(obs[t], amount);
        }
    }

  private:
    std::vector<markov_model> models_;
};
}
}
}
#endif