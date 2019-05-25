#include "misa/bot.h"

#include <cmath>
#include <random>
#include <deque>

template<int A, int N>
double rastrigin(const std::array<double, N> &array) {
    double sum = A * N;
    for (int dim = 0; dim < N; ++dim) {
        double x = array[dim];
        sum += x * x - A * std::cos(2.0 * M_PI * x);
    }
    return sum;
}

const int N = 17;
typedef std::array<double, N> Gene;

class Individual {
public:
    double fitness;
    Gene gene;
};

typedef std::deque<Individual> Population;

struct Comp {
    bool operator()(const Individual &a, const Individual &b) {
        return a.fitness < b.fitness;
    }
};

const int K = 1;
const int P = N * 22;
const int C = N * 8;

Bot bot{};

double calcMisaminoScore(const std::array<double, 17> &array) {
    return -bot.run(array);
//    return rastrigin<10, 17>(array);
}

template<int C>
void generate(
        const std::array<Individual, N + K> &parents,
        std::array<Individual, N + K> &children,
        std::mt19937 &mt199371
) {
    Gene g{};
    for (const auto &parent : parents) {
        for (int dim = 0; dim < N; ++dim) {
            g[dim] += parent.gene[dim];
        }

        for (int dim = 0; dim < N; ++dim) {
            g[dim] /= N;
        }
    }

    std::array<Gene, N + K> diff{};
    for (int index = 0; index < parents.size(); ++index) {
        auto &parent = parents[index];
        for (int dim = 0; dim < N; ++dim) {
            diff[index][dim] = parent.gene[dim] - g[dim];
        }
    }

    std::array<Individual, C> candidates{};
    const double a = std::sqrt(3.0 / (N + K));
    std::uniform_real_distribution<> rand(-a, a);

    for (auto &candidate : candidates) {
        candidate.gene = g;

        for (const auto &d : diff) {
            auto sigma = rand(mt199371);
            for (int dim = 0; dim < N; ++dim) {
                candidate.gene[dim] += sigma * d[dim];
            }
        }

//        candidate.fitness = rastrigin<10, N>(candidate.gene);
        candidate.fitness = calcMisaminoScore(candidate.gene);
    }

    std::sort(candidates.begin(), candidates.end(), Comp());

    for (int i = 0; i < children.size(); ++i) {
        children[i] = candidates[i];
    }
}

int main() {
//    const Gene gene = std::array<double, N>{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
//    std::cout << rastrigin<10, N>(gene) << std::endl;

    std::random_device randomDevice;
    std::mt19937 mt199371(randomDevice());
    std::uniform_real_distribution<> rand(-100.0, 100.0);

    Population population = std::deque<Individual>{};
    for (int index = 0; index < P; ++index) {
        Gene gene;
        for (int dim = 0; dim < N; ++dim) {
            gene[dim] = rand(mt199371);
        }
//        double fitness = rastrigin<10, N>(gene);
        double fitness = calcMisaminoScore(gene);
        population.emplace_back(Individual{fitness, gene});
    }

    // best
    std::cout << "init" << std::endl;
    std::sort(population.begin(), population.end(), Comp());

    std::cout << population[0].fitness << std::endl;
    for (int i = 0; i < N; ++i) {
        std::cout << population[0].gene[i] << ",";
    }
    std::cout << std::endl;

    for (int count = 0; count < 10000; ++count) {
        std::shuffle(population.begin(), population.end(), mt199371);

        // init parents
        auto parents = std::array<Individual, N + K>{};
        for (auto &parent : parents) {
            parent = population.front();
            population.pop_front();
        }

        // create
        auto children = std::array<Individual, N + K>{};
        generate<C>(parents, children, mt199371);
        for (auto &child : children) {
            population.emplace_back(child);
        }

        // best
        std::cout << count << std::endl;

        std::sort(population.begin(), population.end(), Comp());

        std::cout << population[0].fitness << std::endl;
        for (int i = 0; i < N; ++i) {
            std::cout << population[0].gene[i] << ",";
        }
        std::cout << std::endl;
    }

    return 0;
}