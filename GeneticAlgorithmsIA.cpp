#include <SFML/Graphics.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <bitset>
#include <string>
#include <chrono>
#include <cstdio>
#include <cmath>

const int BITS_X = 7;
const int BITS_Y = 6;
const int BITS_TOTAL = BITS_X + BITS_Y;

const int POPULATION_SIZE = 20;
const int GENERATIONS = 30;
const double CROSSOVER_RATE = 0.8;
const double MUTATION_RATE = 0.03;
const int TOURNAMENT_SIZE = 3;
const int PRINT_INTERVAL = 10;

struct Individual {
    std::bitset<BITS_TOTAL> chromosome;
    double fitness = 0.0;
    int x = 0;
    int y = 0;
};

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<double> dis_prob(0.0, 1.0);
std::uniform_int_distribution<int> dis_bit(0, 1);
std::uniform_int_distribution<int> dis_point(1, BITS_TOTAL - 1);
std::uniform_int_distribution<int> dis_pop(0, POPULATION_SIZE - 1);

std::vector<float> best_history;
std::vector<float> avg_history;

Individual global_best;
int global_best_generation = 0;
Individual initial_best;
double initial_avg = 0.0;
double final_avg = 0.0;
double execution_time = 0.0;

double objective_function(int x, int y) {
    double dx = static_cast<double>(x);
    double dy = static_cast<double>(y);
    return (dx * dx) - (2.0 * dx * dy) + (dy * dy);
}

void evaluate(Individual& ind) {
    int x = 0, y = 0;
    for (int i = 0; i < BITS_X; ++i)
        if (ind.chromosome[i]) x |= (1 << i);
    for (int i = 0; i < BITS_Y; ++i)
        if (ind.chromosome[BITS_X + i]) y |= (1 << i);

    ind.x = x;
    ind.y = y;
    ind.fitness = objective_function(x, y);
}

void calculate_statistics(const std::vector<Individual>& population, Individual& best, double& average) {
    double sum = 0.0;
    best = population[0];
    for (const auto& ind : population) {
        sum += ind.fitness;
        if (ind.fitness < best.fitness) best = ind;
    }
    average = sum / population.size();
}

Individual tournament_selection(const std::vector<Individual>& population) {
    Individual best = population[dis_pop(gen)];
    for (int i = 1; i < TOURNAMENT_SIZE; ++i) {
        const Individual& challenger = population[dis_pop(gen)];
        if (challenger.fitness < best.fitness) best = challenger;
    }
    return best;
}

void mutate(Individual& ind) {
    for (int i = 0; i < BITS_TOTAL; ++i)
        if (dis_prob(gen) < MUTATION_RATE) ind.chromosome.flip(i);
}

void print_row(int g, double best, double average) {
    std::cout << std::setw(6) << g
        << std::setw(16) << std::fixed << std::setprecision(2) << best
        << std::setw(16) << average << "\n";
}

void run_genetic_algorithm() {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<Individual> population(POPULATION_SIZE);
    for (auto& ind : population) {
        for (int b = 0; b < BITS_TOTAL; ++b) ind.chromosome[b] = dis_bit(gen);
        evaluate(ind);
    }

    Individual generation_best;
    double average = 0.0;
    calculate_statistics(population, generation_best, average);

    global_best = generation_best;
    global_best_generation = 0;
    initial_best = generation_best;
    initial_avg = average;

    best_history.push_back(static_cast<float>(generation_best.fitness));
    avg_history.push_back(static_cast<float>(average));

    std::cout << "\n" << std::setw(6) << "Gen"
        << std::setw(16) << "Best f(x,y)"
        << std::setw(16) << "Avg f(x,y)" << "\n";
    std::cout << "------------------------------------------\n";
    print_row(0, generation_best.fitness, average);

    for (int g = 1; g <= GENERATIONS; ++g) {
        std::vector<Individual> new_population;
        new_population.reserve(POPULATION_SIZE);

        new_population.push_back(global_best);

        while (static_cast<int>(new_population.size()) < POPULATION_SIZE) {
            Individual p1 = tournament_selection(population);
            Individual p2 = tournament_selection(population);
            Individual child1 = p1, child2 = p2;

            if (dis_prob(gen) < CROSSOVER_RATE) {
                int point = dis_point(gen);
                for (int i = point; i < BITS_TOTAL; ++i) {
                    child1.chromosome[i] = p2.chromosome[i];
                    child2.chromosome[i] = p1.chromosome[i];
                }
            }

            mutate(child1);
            evaluate(child1);
            new_population.push_back(child1);

            if (static_cast<int>(new_population.size()) < POPULATION_SIZE) {
                mutate(child2);
                evaluate(child2);
                new_population.push_back(child2);
            }
        }
        population = new_population;

        calculate_statistics(population, generation_best, average);
        if (generation_best.fitness < global_best.fitness) {
            global_best = generation_best;
            global_best_generation = g;
        }

        best_history.push_back(static_cast<float>(generation_best.fitness));
        avg_history.push_back(static_cast<float>(average));

        if (g % PRINT_INTERVAL == 0) print_row(g, generation_best.fitness, average);
    }
    final_avg = average;

    auto end_time = std::chrono::high_resolution_clock::now();
    execution_time = std::chrono::duration<double>(end_time - start_time).count();
}

void print_results() {
    const std::string line(62, '-');

    std::cout << "\n" << line << "\n";
    std::cout << " INITIAL GENERATION\n";
    std::cout << line << "\n";
    std::cout << "Best individual: x = " << initial_best.x << ", y = " << initial_best.y
        << ", f(x,y) = " << std::fixed << std::setprecision(2) << initial_best.fitness << "\n";
    std::cout << "Initial generation average: " << initial_avg << "\n";

    std::cout << "\n" << line << "\n";
    std::cout << " FINAL OUTPUT\n";
    std::cout << line << "\n";
    std::cout << "Best solution: x = " << global_best.x << ", y = " << global_best.y << "\n";
    std::cout << "Minimum value: f(x,y) = " << std::fixed << std::setprecision(2) << global_best.fitness << "\n";
    std::cout << "Chromosome: " << global_best.chromosome << "\n";
    std::cout << "Found at generation: " << global_best_generation << "\n";
    std::cout << "Final generation average: " << final_avg << "\n";
    std::cout << "Execution time: " << std::fixed << std::setprecision(6) << execution_time << " seconds\n";
    std::cout << line << "\n";
}

int main() {
    std::cout << "--------------------------------------------------------------\n";
    std::cout << " GENETIC ALGORITHM - MINIMIZATION f(x,y) = x^2 - 2xy + y^2\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "Domain: x in [0,127] (7 bits), y in [0,63] (6 bits)\n";
    std::cout << "Chromosome: " << BITS_TOTAL << " bits\n";
    std::cout << "Population: " << POPULATION_SIZE << " individuals\n";
    std::cout << "Generations: " << GENERATIONS << "\n";
    std::cout << "Crossover rate: " << CROSSOVER_RATE << " | Mutation rate: " << MUTATION_RATE << "\n";
    std::cout << "Elitism: 1 individual | Tournament of " << TOURNAMENT_SIZE << "\n";
    std::cout << "--------------------------------------------------------------\n";

    run_genetic_algorithm();
    print_results();

    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;

    sf::RenderWindow window(sf::VideoMode(900, 600), "Genetic Algorithm", sf::Style::Default, settings);

    sf::Font font;
    if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
        std::cerr << "Failed to load font from C:/Windows/Fonts/arial.ttf\n";
        return -1;
    }

    const float W = static_cast<float>(window.getSize().x);
    const float H = static_cast<float>(window.getSize().y);
    const float padLeft = 80.0f, padBottom = 60.0f, padRight = 40.0f, padTop = 40.0f;
    const float gWidth = W - padLeft - padRight;
    const float gHeight = H - padBottom - padTop;
    const float baseY = H - padBottom;

    float max_fitness = 1.0f;
    for (float f : avg_history) if (f > max_fitness) max_fitness = f;
    for (float f : best_history) if (f > max_fitness) max_fitness = f;

    float log_max = std::log10(max_fitness + 1.0f);

    const int n = static_cast<int>(avg_history.size());
    sf::VertexArray avg_line(sf::LineStrip, n);
    sf::VertexArray best_line(sf::LineStrip, n);
    for (int i = 0; i < n; ++i) {
        float x = padLeft + (i * gWidth / (n - 1));

        float y_avg = baseY - (std::log10(avg_history[i] + 1.0f) / log_max) * gHeight;
        float y_best = baseY - (std::log10(best_history[i] + 1.0f) / log_max) * gHeight;

        avg_line[i].position = sf::Vector2f(x, y_avg);
        avg_line[i].color = sf::Color(50, 100, 255);
        best_line[i].position = sf::Vector2f(x, y_best);
        best_line[i].color = sf::Color::Red;
    }

    sf::VertexArray axes(sf::Lines, 4);
    axes[0].position = sf::Vector2f(padLeft, baseY);
    axes[1].position = sf::Vector2f(W - padRight, baseY);
    axes[2].position = sf::Vector2f(padLeft, baseY);
    axes[3].position = sf::Vector2f(padLeft, padTop);

    for (int i = 0; i < 4; ++i) axes[i].color = sf::Color::Black;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        window.clear(sf::Color::White);
        window.draw(axes);

        const int numTicksY = 5;
        for (int i = 0; i <= numTicksY; ++i) {
            float current_log = i * (log_max / numTicksY);
            float y = baseY - (current_log / log_max) * gHeight;
            float val = std::pow(10.0f, current_log) - 1.0f;

            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.0f", val);
            sf::Text text(buffer, font, 14);
            text.setFillColor(sf::Color::Black);
            sf::FloatRect bounds = text.getLocalBounds();
            text.setPosition(padLeft - bounds.width - 10.0f, y - bounds.height / 2.0f - 4.0f);
            window.draw(text);
        }

        const int numTicksX = 10;
        for (int i = 0; i <= numTicksX; ++i) {
            float x = padLeft + (i * gWidth / numTicksX);

            int gen_val = (GENERATIONS * i) / numTicksX;
            sf::Text text(std::to_string(gen_val), font, 14);
            text.setFillColor(sf::Color::Black);
            sf::FloatRect bounds = text.getLocalBounds();
            text.setPosition(x - bounds.width / 2.0f, baseY + 10.0f);
            window.draw(text);
        }

        sf::Text titleX("Generations", font, 16);
        titleX.setFillColor(sf::Color::Black);
        titleX.setPosition(W / 2.0f - 50.0f, H - 25.0f);
        window.draw(titleX);

        sf::Text titleY("f(x,y) [Log Scale]", font, 16);
        titleY.setFillColor(sf::Color::Black);
        titleY.setPosition(10.0f, padTop - 30.0f);
        window.draw(titleY);

        sf::Text best_legend("Minimization", font, 14);
        best_legend.setFillColor(sf::Color::Red);
        best_legend.setPosition(W - 200.0f, 10.0f);
        window.draw(best_legend);

        sf::Text avg_legend("Average", font, 14);
        avg_legend.setFillColor(sf::Color(50, 100, 255));
        avg_legend.setPosition(W - 200.0f, 30.0f);
        window.draw(avg_legend);

        window.draw(avg_line);
        window.draw(best_line);

        window.display();
    }

    return 0;
}