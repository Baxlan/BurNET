// cost.cpp

#include "omnilearn/Activation.hh"
#include "omnilearn/cost.h"



omnilearn::Matrix omnilearn::L1Loss(Matrix const& real, Matrix const& predicted, ThreadPool& t)
{
    Matrix loss(real.rows(), real.cols());
    std::vector<std::future<void>> tasks(loss.rows());
    for(eigen_size_t i = 0; i < loss.rows(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &loss, i]()->void
        {
            for(eigen_size_t j = 0; j < loss.cols(); j++)
            {
                loss(i, j) = std::abs(real(i, j) - predicted(i, j));
            }
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return loss;
}


omnilearn::Vector omnilearn::L1Grad(Vector const& real, Vector const& predicted, ThreadPool& t)
{
    Vector gradients(real.size());
    std::vector<std::future<void>> tasks(real.size());
    for(eigen_size_t i = 0; i < real.size(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &gradients, i]()->void
        {
            if (real(i) < predicted(i))
                gradients(i) = -1;
            else if (real(i) > predicted(i))
                gradients(i) = 1;
            else
                gradients(i) = 0;
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return gradients;
}


omnilearn::Matrix omnilearn::L2Loss(Matrix const& real, Matrix const& predicted, ThreadPool& t)
{
    Matrix loss(real.rows(), real.cols());
    std::vector<std::future<void>> tasks(loss.rows());
    for(eigen_size_t i = 0; i < loss.rows(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &loss, i]()->void
        {
            for(eigen_size_t j = 0; j < loss.cols(); j++)
            {
                loss(i, j) = 0.5 * std::pow(real(i, j) - predicted(i, j), 2);
            }
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return loss;
}


omnilearn::Vector omnilearn::L2Grad(Vector const& real, Vector const& predicted, ThreadPool& t)
{
    Vector gradients(real.size());
    std::vector<std::future<void>> tasks(real.size());
    for(eigen_size_t i = 0; i < real.size(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &gradients, i]()->void
        {
            gradients(i) = (real(i) - predicted(i));
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return gradients;
}


omnilearn::Matrix omnilearn::crossEntropyLoss(Matrix const& real, Matrix const& predicted, double crossEntropyBias, bool useWeights, Vector weights, ThreadPool& t)
{
    Matrix softMax = softmax(predicted);
    Matrix loss(real.rows(), real.cols());
    std::vector<std::future<void>> tasks(loss.rows());
    for(eigen_size_t i = 0; i < loss.rows(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &useWeights, &weights, &crossEntropyBias, &softMax, &loss, i]()->void
        {
            for(eigen_size_t j = 0; j < loss.cols(); j++)
            {
                loss(i, j) = real(i, j) * -std::log(softMax(i, j) + crossEntropyBias);
                if(useWeights)
                    loss(i, j) /= (std::abs(real(i, j)-1) <= std::numeric_limits<double>::epsilon() ? -std::log(1-weights(j))/std::log(2) : -std::log(weights(j))/std::log(2));
                    // dividing by ln(2) to get base 2 logarithm, thus if ratio (aka weight) is 50%, the weighting factor is 1 (no weighting)
            }
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return loss;
}


omnilearn::Vector omnilearn::crossEntropyGrad(Vector const& real, Vector const& predicted, bool useWeights, Vector weights, ThreadPool& t)
{
    Vector softMax = singleSoftmax(predicted);
    Vector gradients(real.size());
    std::vector<std::future<void>> tasks(real.size());
    for(eigen_size_t i = 0; i < real.size(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &useWeights, &weights, &softMax, &gradients, i]()->void
        {
            gradients(i) = real(i) - softMax(i);
            if(useWeights)
                gradients(i) /= (std::abs(real(i)-1) <= std::numeric_limits<double>::epsilon() ? -std::log(1-weights(i))/std::log(2) : -std::log(weights(i))/std::log(2));
                // dividing by ln(2) to get base 2 logarithm, thus if ratio (aka weight) is 50%, the weighting factor is 1 (no weighting)
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return gradients;
}


omnilearn::Matrix omnilearn::binaryCrossEntropyLoss(Matrix const& real, Matrix const& predicted, double crossEntropyBias, bool useWeights, Vector weights, ThreadPool& t)
{
    Matrix loss(real.rows(), real.cols());
    std::vector<std::future<void>> tasks(loss.rows());
    for(eigen_size_t i = 0; i < loss.rows(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &useWeights, &weights, &crossEntropyBias, &loss, i]()->void
        {
            for(eigen_size_t j = 0; j < loss.cols(); j++)
            {
                loss(i, j) = -(real(i, j) * std::log(predicted(i, j) + crossEntropyBias) + (1 - real(i, j)) * std::log(1 - predicted(i, j) + crossEntropyBias));
                if(useWeights)
                    loss(i, j) /= (std::abs(real(i, j)-1) <= std::numeric_limits<double>::epsilon() ? -std::log(1-weights(j))/std::log(2) : -std::log(weights(j))/std::log(2));
                    // dividing by ln(2) to get base 2 logarithm, thus if ratio (aka weight) is 50%, the weighting factor is 1 (no weighting)
            }
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return loss;
}


omnilearn::Vector omnilearn::binaryCrossEntropyGrad(Vector const& real, Vector const& predicted, double crossEntropyBias, bool useWeights, Vector weights, ThreadPool& t)
{
    Vector gradients(real.size());
    std::vector<std::future<void>> tasks(real.size());
    for(eigen_size_t i = 0; i < real.size(); i++)
    {
        tasks[i] = t.enqueue([&real, &predicted, &crossEntropyBias, &useWeights, &weights, &gradients, i]()->void
        {
            gradients(i) = (real(i) - predicted(i)) / ((predicted(i) * (1 -  predicted(i))) + crossEntropyBias);
            if(useWeights)
                gradients(i) /= (std::abs(real(i)-1) <= std::numeric_limits<double>::epsilon() ? -std::log(1-weights(i))/std::log(2) : -std::log(weights(i))/std::log(2));
                // dividing by ln(2) to get base 2 logarithm, thus if ratio (aka weight) is 50%, the weighting factor is 1 (no weighting)
        });
    }
    for(size_t i = 0; i < tasks.size(); i++)
        tasks[i].get();
    return gradients;
}