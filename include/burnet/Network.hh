#ifndef BURNET_NETWORK_HH_
#define BURNET_NETWORK_HH_

#include "Layer.hh"

#include <iostream>

namespace burnet
{

//=============================================================================
//=============================================================================
//=============================================================================
//=== NETWORK =================================================================
//=============================================================================
//=============================================================================
//=============================================================================


class Network
{
public:
  Network(Dataset data = Dataset(), NetworkParam const& param = NetworkParam()):
  _dataSeed(param.dataSeed == 0 ? static_cast<unsigned>(std::chrono::steady_clock().now().time_since_epoch().count()) : param.dataSeed),
  _dataGen(std::mt19937(_dataSeed)),
  _layers(),
  _decay(param.decay),
  _batchSize(param.batchSize),
  _learningRate(param.learningRate),
  _L1(param.L1),
  _L2(param.L2),
  _tackOn(param.tackOn),
  _maxEpoch(param.maxEpoch),
  _epochAfterOptimal(param.epochAfterOptimal),
  _loss(param.loss),
  _validationRatio(param.validationRatio),
  _testRatio(param.testRatio),
  _trainData(data),
  _validationData(),
  _validationRealResults(),
  _testData(),
  _testRealResults(),
  _nbBatch(0),
  _epoch(0),
  _optimalEpoch(0),
  _trainLosses(std::vector<double>()),
  _validLosses(std::vector<double>()),
  _testAccuracy(std::vector<double>())
  {
  }


  Network(NetworkParam const& param = NetworkParam(), Dataset data = Dataset()):
  Network(data, param)
  {
  }


  template <typename Aggr_t = Dot, typename Act_t = Relu>
  void addLayer(LayerParam const& param = LayerParam())
  {
    _layers.push_back(std::make_shared<Layer<Aggr_t, Act_t>>(param));
  }


  void setData(std::vector<std::pair<std::vector<double>, std::vector<double>>> data)
  {
    _trainData = data;
  }


  std::pair<Matrix, Matrix> computeLossMatrix(Matrix const& realResults, Matrix const& predicted)
  {
    if(_loss == Loss::L1)
      return L1Loss(realResults, predicted);
    else if(_loss == Loss::L2)
      return L2Loss(realResults, predicted);
    else if(_loss == Loss::Entropy)
      return entropyLoss(realResults, predicted);
    else
      return {};
  }


  void learn()
  {
    initLayers();
    shuffleData();

    if(_layers[_layers.size()-1]->size() != _trainData[0].second.size())
    {
      throw Exception("The last layer must have as much neurons as outputs.");
    }

    double lowestLoss = computeLoss();
    std::cout << "\n";
    for(_epoch = 1; _epoch < _maxEpoch; _epoch++)
    {
      for(unsigned batch = 0; batch < _nbBatch; batch++)
      {
        Matrix input(_batchSize);
        Matrix output(_batchSize);
        for(unsigned i = 0; i < _batchSize; i++)
        {
          input[i] = _trainData[batch*_batchSize+i].first;
          output[i] = _trainData[batch*_batchSize+i].second;
        }

        for(unsigned i = 0; i < _layers.size(); i++)
        {
          input = _layers[i]->processToLearn(input);
        }

        Matrix gradients(transpose(computeLossMatrix(output, input).second));
        for(unsigned i = 0; i < _layers.size(); i++)
        {
          _layers[_layers.size() - i - 1]->computeGradients(gradients);
          gradients = _layers[_layers.size() - i - 1]->getGradients();
        }
        for(unsigned i = 0; i < _layers.size(); i++)
        {
          _layers[i]->updateWeights(_decay(_learningRate, _epoch), _L1, _L2, _tackOn, 0);
        }
      }
      std::cout << "Epoch: " << _epoch;
      double loss = computeLoss();
      std::cout << "   LR: " << _decay(_learningRate, _epoch) << "\n";
      if(loss - lowestLoss < -std::numeric_limits<double>::max())
      {
        save();
        _optimalEpoch = _epoch;
      }
      if(_epoch - _optimalEpoch > _epochAfterOptimal)
        break;
    }
    loadSaved();
    std::cout << "\nOptimal epoch: " << _optimalEpoch << "   Accuracy: " << _testAccuracy[_optimalEpoch] << "%\n";

  }


  void save()
  {
     for(unsigned i = 0; i < _layers.size(); i++)
      {
          _layers[i]->save();
      }
  }


  void loadSaved()
  {
     for(unsigned i = 0; i < _layers.size(); i++)
      {
          _layers[i]->loadSaved();
      }
  }


  //return loss
  double computeLoss()
  {
    Matrix input(_trainData.size());
    Matrix output(_trainData.size());
    for(unsigned i = 0; i < _trainData.size(); i++)
    {
      input[i] = _trainData[i].first;
      output[i] = _trainData[i].second;
    }
    input = process(input);
    double trainLoss = averageLoss(computeLossMatrix(output, input).first);

    Matrix validationResult = process(_validationData);
    double validationLoss = averageLoss(computeLossMatrix(_validationRealResults, validationResult).first);

    Matrix testResult = process(_testData);
    double testAccuracy = accuracy(_testRealResults, testResult, 0.2);

    std::cout << "   Valid_Loss: " << validationLoss << "   Train_Loss: " << trainLoss << "   Accuracy: " << testAccuracy << "%";
    _trainLosses.push_back(trainLoss);
    _validLosses.push_back(validationLoss);
    _testAccuracy.push_back(testAccuracy);
    return validationLoss;
  }


  Matrix process(Matrix inputs) const
  {
    for(unsigned i = 0; i < _layers.size(); i++)
    {
      inputs = _layers[i]->process(inputs);
    }
    // if cross-entropy loss is used, then score must be softmax
    if(_loss == Loss::Entropy)
    {
      inputs = softmax(inputs);
    }
    return inputs;
  }


protected:
  void initLayers()
  {
    for(unsigned i = 0; i < _layers.size(); i++)
    {
        _layers[i]->init((i == 0 ? _trainData[0].first.size() : _layers[i-1]->size()),
                        (i == _layers.size()-1 ? _trainData[0].second.size() : _layers[i+1]->size()),
                        _batchSize);
    }
  }


  void shuffleData()
  {
    //testData(); //tests if all data have the same number of inputs and of output
    std::shuffle(_trainData.begin(), _trainData.end(), _dataGen);

    double validation = _validationRatio * _trainData.size();
    double test = _testRatio * _trainData.size();
    double nbBatch = std::trunc(_trainData.size() - validation - test) / _batchSize;

    //add a batch if an incomplete batch has more than 0.5*batchsize data
    if(nbBatch - static_cast<unsigned>(nbBatch) >= 0.5)
      nbBatch = std::trunc(nbBatch) + 1;

    unsigned nbTrain = static_cast<unsigned>(nbBatch)*_batchSize;
    unsigned noTrain = _trainData.size() - nbTrain;
    validation = std::round(noTrain*_validationRatio/(_validationRatio + _testRatio));
    test = std::round(noTrain*_testRatio/(_validationRatio + _testRatio));

    for(unsigned i = 0; i < static_cast<unsigned>(validation); i++)
    {
      _validationData.push_back(_trainData[_trainData.size()-1].first);
      _validationRealResults.push_back(_trainData[_trainData.size()-1].second);
      _trainData.pop_back();
    }
    for(unsigned i = 0; i < static_cast<unsigned>(test); i++)
    {
      _testData.push_back(_trainData[_trainData.size()-1].first);
      _testRealResults.push_back(_trainData[_trainData.size()-1].second);
      _trainData.pop_back();
    }
    _nbBatch = static_cast<unsigned>(nbBatch);
  }


protected:
  unsigned _dataSeed;
  std::mt19937 _dataGen;

  std::vector<std::shared_ptr<ILayer>> _layers;

  double (* _decay)(double, unsigned);

  unsigned const _batchSize;
  double _learningRate;
  double _L1;
  double _L2;
  double _tackOn;
  unsigned const _maxEpoch;
  unsigned const _epochAfterOptimal;
  Loss _loss;

  double _validationRatio;
  double _testRatio;
  Dataset _trainData;
  Matrix _validationData;
  Matrix _validationRealResults;
  Matrix _testData;
  Matrix _testRealResults;
  unsigned _nbBatch;

  unsigned _epoch;
  unsigned _optimalEpoch;
  std::vector<double> _trainLosses;
  std::vector<double> _validLosses;
  std::vector<double> _testAccuracy;
};



} // namespace burnet

#endif //BURNET_NETWORK_HH_