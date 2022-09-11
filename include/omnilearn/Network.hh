// Network.hh

#ifndef OMNILEARN_NETWORK_HH_
#define OMNILEARN_NETWORK_HH_

#include "NetworkIO.hh"
#include "Layer.hh"
#include "csv.h"

#include <filesystem>



namespace fs = std::filesystem;



namespace omnilearn
{

enum class Loss {L1, L2, CrossEntropy, BinaryCrossEntropy};
enum class Metric {L1, L2, Accuracy};
enum class Preprocess {Center, Normalize, Standardize, Decorrelate, Whiten, Reduce, Recorrelate};
enum class Scheduler {None, Exp, Step, Plateau};
enum class SecondOrder {None, Univariate, Multivariate};



//=============================================================================
//=============================================================================
//=============================================================================
//=== NETWORK PARAMETERS ======================================================
//=============================================================================
//=============================================================================
//=============================================================================



struct NetworkParam
{
    NetworkParam():
    seed(0),
    batchSize(1),
    useBatchSizeScheduler(false),
    maxBatchSizeRatio(0.1),
    learningRate(0.01),
    L1(0),
    L2(0),
    decay(0), // weight decay
    epoch(1000),
    patience(5),
    dropout(0),
    dropconnect(0),
    validationRatio(0.2),
    testRatio(0.2),
    loss(Loss::L2),
    schedulerValue(2),
    schedulerDelay(2),
    scheduler(Scheduler::None),
    classificationThreshold(0.5),
    threads(1),
    automaticLearningRate(false),
    adaptiveLearningRate(false),
    momentum(0),
    maxMomentum(0.9),
    momentumSchedulerDelay(1),
    momentumSchedulerValue(1),
    momentumScheduler(Scheduler::None),
    window(0.99),
    improvement(0.01),
    preprocessInputs(),
    preprocessOutputs(),
    optimizerBias(1e-6),
    inputReductionThreshold(0.9995),
    outputReductionThreshold(0.9995),
    inputWhiteningBias(1e-5),
    name("omnilearn_network"),
    verbose(false)
    {
    }

    unsigned seed;
    size_t batchSize;
    bool useBatchSizeScheduler; // use the same scheduler, delay and value than LR ones
    double maxBatchSizeRatio;
    double learningRate;
    double L1;
    double L2;
    double decay;
    size_t epoch;
    size_t patience;
    double dropout;
    double dropconnect;
    double validationRatio;
    double testRatio;
    Loss loss;
    double schedulerValue;
    size_t schedulerDelay;
    Scheduler scheduler;
    double classificationThreshold;
    size_t threads;
    bool automaticLearningRate;
    bool adaptiveLearningRate;
    double momentum; //momentum
    double maxMomentum; //asymptotic value the momentum tries to reach in case of momentum shedule
    size_t momentumSchedulerDelay;
    double momentumSchedulerValue;
    Scheduler momentumScheduler;
    double window; //b2 in the second moment of gradients (and of updates)
    double improvement; // minimum validation loss improvement nedeed to become the new optimal
    std::vector<Preprocess> preprocessInputs;
    std::vector<Preprocess> preprocessOutputs;
    double optimizerBias;
    double inputReductionThreshold;
    double outputReductionThreshold;
    double inputWhiteningBias;
    std::string name;
    bool verbose;
};



//=============================================================================
//=============================================================================
//=============================================================================
//=== NETWORK =================================================================
//=============================================================================
//=============================================================================
//=============================================================================



class Network
{
  friend class NetworkIO; // NetworkIO is part of Network

public:
  Network() = default;
  void load(fs::path const& path, size_t threads);
  void addLayer(LayerParam const& param);
  void setParam(NetworkParam const& param);
  void setData(Data const& data);
  void setTestData(Data const& data);
  void learn();
  Vector process(Vector inputs) const;
  Matrix process(Matrix inputs) const;
  Vector generate(NetworkParam param, Vector target, Vector input = Vector(0));

  Vector preprocess(Vector inputs) const; //transforms real inputs to processed inputs
  Vector postprocess(Vector inputs) const; //transforms produced outputs to real outputs
  Vector depreprocess(Vector inputs) const; //transforms processed inputs to real inputs
  Vector depostprocess(Vector inputs) const; //transforms real outputs to produced outputs
  Matrix preprocess(Matrix inputs) const;
  Matrix postprocess(Matrix inputs) const;
  Matrix depreprocess(Matrix inputs) const;
  Matrix depostprocess(Matrix inputs) const;

private:
  void initLayers();
  void splitData(); // shuffle data then split them into train/validation/test data
  void shuffleTrainData(); // shuffle train data each epoch
  void initPreprocess(); // first preprocess : calculate and store all the preprocessing data
  void performeOneEpoch();
  Matrix processForLoss(Matrix inputs) const; //takes preprocessed inputs, returns postprocessed outputs
  Matrix computeLossMatrix(Matrix const& realResult, Matrix const& predicted) const;
  Vector computeGradVector(Vector const& realResult, Vector const& predicted) const; // calculate error between expected and predicted outputs
  void computeLoss();
  void keep(); // store weights, bias and other coefs when optimal loss is found
  void release(); // release weights, bias and other coefs when learning is done
  void adaptLearningRate();
  void adaptBatchSize();
  void adaptMomentum();
  void check() const;
  void list(double lowestLoss, bool initial) const;

private:
  //parameters
  NetworkParam _param;

  //random generators
  std::mt19937 _generator;
  std::bernoulli_distribution _dropoutDist;
  std::bernoulli_distribution _dropconnectDist;

  //layers of neurons
  std::vector<Layer> _layers;

  //threadpool for parallelization
  mutable std::unique_ptr<ThreadPool> _pool; // must be a pointer to be able to re-construct the pool (non copyable because of mutex)

  //data
  Matrix _trainInputs;
  Matrix _trainOutputs;
  Matrix _validationInputs;
  Matrix _validationOutputs;
  Matrix _testInputs;
  Matrix _testOutputs;
  Matrix _testRawInputs;
  Matrix _testRawOutputs;
  Matrix _testNormalizedOutputsForMetric;

  //learning infos
  size_t _epoch;
  size_t _optimalEpoch;
  size_t _iteration;
  double _currentLearningRate;
  double _currentMomentum;
  double _previousMomentum;
  double _nextMomentum;
  double _cumulativeMomentum;
  size_t _currentBatchSize;
  size_t _nbBatch;
  size_t _missedData; // # of data ignored because the minibatch would be incomplete
  size_t _epochWhenBatchSizeReachedMax;
  Vector _trainLosses;
  Vector _validLosses;
  Vector _testMetric;
  Vector _testSecondMetric;
  Vector _testThirdMetric;
  Vector _testFourthMetric;

  //labels
  std::vector<std::string> _inputLabels;
  std::vector<std::string> _outputLabels;

  //output preprocessing
  Vector _outputCenter;
  std::vector<std::pair<double, double>> _outputNormalization;
  std::pair<Matrix, Vector> _outputDecorrelation;
  std::vector<std::pair<double, double>> _metricNormalization;

  //input preprocessing
  Vector _inputCenter;
  std::vector<std::pair<double, double>> _inputNormalization;
  std::vector<std::pair<double, double>> _inputStandartization;
  std::pair<Matrix, Vector> _inputDecorrelation;

  //IO
  std::unique_ptr<NetworkIO> _io; // is a pointer because we only need IO during training
};



} // namespace omnilearn

#endif //OMNILEARN_NETWORK_HH_