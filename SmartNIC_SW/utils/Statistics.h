#ifndef __statistics_h__
#define __statistics_h__

/*
***************************************************************************
*
* Copyright (c) 2008-2019, Silicom Denmark A/S
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the Silicom nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* 4. This software may only be redistributed and used in connection with a
*  Silicom network adapter product.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
***************************************************************************/

#ifdef __cplusplus

// C headers
#include <stdint.h>

// C++ headers
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include "CommonUtilities.h"

/**
 *  Accumulate miscellaneous statistics entities like average, standard deviation, minimum and maximum, etc.
 */
class Statistics
{
public:

    /**
    *  Statistics collection types.
    */
    enum Extent_t
    {
        COLLECT_BASIC = 0x1,                            ///< Collect basic statistics only.
        COLLECT_ADVANCED = 0x2,                         ///< Collect advanced statistics.
        COLLECT_ALL_SAMPLES = 0x4 | COLLECT_ADVANCED    ///< Collect all samples, implies also advanced statistics
    };

private:

    struct
    {
        Extent_t                Extent;             ///< How much to collect?
        double                  Count;              ///< Double count for computations.
        uint64_t                IntegerCount;       ///< Integer count for the public interface.
        double                  Average, PreviousAverage;   ///< For computing floating average.
        double                  Q, PreviousQ;       ///< For computing of floating standard deviation.
        double                  Sum, SquaredSum, CubicSum, QuadraticSum, QuintileSum;  ///< For computing moments, skewness and kurtosis
        double                  Minimum;            ///< Minimum sample value.
        double                  Maximum;            ///< Maximum sample value.

    }                           _values;

    std::vector<double>         Samples;           ///< All sample values

public:

    double NaN;                             ///< Not a Number (https://en.wikipedia.org/wiki/NaN)

    bool                ThrowExceptions;    ///< Throw exceptions instead of returning NaN.
    std::string         Name;               ///< Name of this statistics.
    const Extent_t &    Extent;             ///< How much to collect?
    const uint64_t &    Count;              ///< Number of samples
    const double &      Average;            ///< Publicly visible average value of samples.
    const double &      Mean;               ///< Alternative name for average.
    double              DifferenceTo;       ///< Set this value if you want to measure difference to a non-zero value;

    /**
     *  Initialize an instance of statistics;
     */
    explicit Statistics()
        : _values()
        , Samples()
        , NaN(nan(""))
        , ThrowExceptions(false)
        , Name()
        , Extent(_values.Extent)
        , Count(_values.IntegerCount)
        , Average(_values.Average)
        , Mean(_values.Average)
        , DifferenceTo(0.0)
    {
        Clear();

        _values.Extent = COLLECT_BASIC;
    }

    /**
    *  Initialize an instance of statistics;
    *
    *   @param  extent      Select statistics collection type.
    */
    explicit Statistics(Statistics::Extent_t extent)
        : _values()
        , Samples()
        , NaN(nan(""))
        , ThrowExceptions(false)
        , Name()
        , Extent(_values.Extent)
        , Count(_values.IntegerCount)
        , Average(_values.Average)
        , Mean(_values.Average)
        , DifferenceTo(0.0)
    {
        Clear();

        _values.Extent = extent;
    }

    /**
    *  Statistics copy constructor.
    *
    *  @param  statistics  Reference to statistics instance to copy construct from.
    *
    *  @return Return reference to this instance (this is required for chained assignments).
    */
    explicit Statistics(const Statistics & statistics)
        : _values(statistics._values)
        , Samples(statistics.Samples)
        , NaN(statistics.NaN)
        , ThrowExceptions(statistics.ThrowExceptions)
        , Name(statistics.Name)
        , Extent(_values.Extent)
        , Count(_values.IntegerCount)
        , Average(_values.Average)
        , Mean(_values.Average)
        , DifferenceTo(0.0)
    {
    }

    /**
     *  Statistics assignment operator.
     *
     *  @param  statistics  Reference to statistics instance to assign to this instance.
     *
     *  @return Return reference to this instance (this is required for chained assignments).
     */
    Statistics & operator=(const Statistics & statistics)
    {
        if (this != &statistics)
        {
            new (this) Statistics(statistics); // Copy construct in place!
        }

        return *this;
    }

    /**
     *  Reset all statistics to initial state.
     */
    inline void Reset()
    {
        Clear();
    }

    /**
    *  Clear all statistics to initial state.
    */
    void Clear()
    {
        memset(&_values, 0, sizeof(_values));
        _values.Minimum = std::numeric_limits<double>::max();
        _values.Maximum = std::numeric_limits<double>::min();
        Samples.clear();
    }

    /**
     *  Add a single sample to the statistics.
     *
     *  @param      x   Sample value.
     */
    void Add(double x)
    {
        x -= DifferenceTo;

        // Floating average:
        _values.PreviousAverage = _values.Average;
        _values.Average = (_values.Count * _values.Average + x) / (_values.Count + 1.0);

        // Floating standard deviation:
        // See https://en.wikipedia.org/wiki/Standard_deviation#Rapid_calculation_methods
        _values.PreviousQ = _values.Q;
        _values.Q = _values.PreviousQ + (x - _values.PreviousAverage) * (x - _values.Average);

        // Minimum and maximum
        if (x < _values.Minimum)
        {
            _values.Minimum = x;
        }
        if (x > _values.Maximum)
        {
            _values.Maximum = x;
        }

        if (_values.Extent & COLLECT_ADVANCED)
        {
            // Collect various sums for computation of higher statistics entities:

            double squaredTerm = x * x;
            double quadraticTerm = squaredTerm * squaredTerm;

            _values.Sum += x;                              // sum(x)
            _values.SquaredSum += squaredTerm;             // sum(x^2)
            _values.CubicSum += squaredTerm * x;           // sum(x^3)
            _values.QuadraticSum += quadraticTerm;         // sum(x^4)
            _values.QuintileSum += quadraticTerm * x;      // sum(x^5)
        }

        ++_values.IntegerCount;
        ++_values.Count;

        if (_values.Extent & COLLECT_ALL_SAMPLES)
        {
            Samples.push_back(x);
        }
    }

    /**
     *  Add a collection of values to the statistics.
     *
     *  @param      pBegin      Pointer to start of collection.
     *  @param      pEnd        Pointer to one past end of collection
     *                          (like STL begin() and end() iterators).
     */
    void Add(const double * pBegin, const double * pEnd)
    {
        assert(pBegin <= pEnd);

        while (pBegin != pEnd)
        {
            Add(*pBegin++);
        }
    }

    /**
    *  Add a single sample to the statistics under a scoped lock.
    *
    *  @param      x   Sample value.
    */
    template <typename T> void Add(double x, T & lock)
    {
        SC_ScopedLock<T> scopedLock(lock);

        Add(x);
    }

    /**
    *  Compute standard deviation of all samples.
    *
    *  @return Sample standard deviation.
    */
    double SampleStandardDeviation() const
    {
        if (_values.Count > 1.0)
        {
            return std::sqrt(_values.Q / (_values.Count - 1.0));
        }
        if (ThrowExceptions)
        {
            std::stringstream ss;
            ss << "Statistics::SampleStandardDeviation(): need at least 2 samples instead of " << _values.Count << " to compute standard deviation";
            throw std::runtime_error(ss.str());
        }
        return NaN;
    }

    /**
    *  Compute standard deviation of all samples.
    *
    *   @brief  This is same as @ref SampleStandardDeviation.
    *
    *  @return Sample standard deviation.
    */
    inline double SD() const
    {
        return SampleStandardDeviation();
    }

    /**
    *  Compute standard deviation of all samples as a percentage of mean.
    *
    *  @return Sample standard deviation as percentage of mean.
    */
    double Percentage_SD() const
    {
        if (Mean == 0.0)
        {
            if (ThrowExceptions)
            {
                std::stringstream ss;
                ss << "Statistics::Percentage_SD(): cannot compute standard deviation percentage if mean is zero";
                throw std::runtime_error(ss.str());
            }
            return NaN;
        }
        return 100.0 * SampleStandardDeviation() / Mean;
    }

    /**
    *  Compute minimum of all samples.
    *
    *  @return Minimum sample value.
    */
    double Minimum() const
    {
        if (_values.Count == 0)
        {
            if (ThrowExceptions)
            {
                std::stringstream ss;
                ss << "Statistics::Minimum(): no samples and therefor no minimum";
                throw std::runtime_error(ss.str());
            }
            return NaN;
        }
        return _values.Minimum;
    }

    /**
    *  Compute maximum of all samples.
    *
    *  @return Maximum sample value.
    */
    double Maximum() const
    {
        if (_values.Count == 0)
        {
            if (ThrowExceptions)
            {
                std::stringstream ss;
                ss << "Statistics::Maximum(): no samples and therefor no maximum";
                throw std::runtime_error(ss.str());
            }
            return NaN;
        }
        return _values.Maximum;
    }

    /**
     *  Compute standard deviation of a population.
     *
     *  @return Population standard deviation.
     */
    double PopulationStandardDeviation() const
    {
        if (_values.Count > 0.0)
        {
            return std::sqrt(_values.Q / _values.Count);
        }
        if (ThrowExceptions)
        {
            std::stringstream ss;
            ss << "Statistics::PopulationStandardDeviation(): need at least one sample to compute population standard deviation";
            throw std::runtime_error(ss.str());
        }
        return NaN;
    }

    /**
     *  Compute kth central moment or moment about mean.
     *
     *  @param  Moment order (k = 1-4)
     *
     *  @return Central moment of kth order.
     */
    double Moment(int k) const
    {
        if (_values.Count > 0.0)
        {
            switch (k)
            {
                case 1: return _values.Sum / _values.Count;
                case 2: return (_values.SquaredSum - 2.0 * _values.Sum * _values.Average + _values.Average * _values.Average) / _values.Count;
                case 3:
                    {
                        double averageSquared = _values.Average * _values.Average;
                        return (_values.CubicSum - 3.0 * _values.SquaredSum * _values.Average + 3.0 * _values.Sum * averageSquared - averageSquared * _values.Average) / _values.Count;
                    }
                case 4:
                    {
                        double averageSquared = _values.Average * _values.Average;
                        return (_values.QuadraticSum - 4.0 * _values.CubicSum * _values.Average + 6.0 * _values.SquaredSum * averageSquared - 4.0 * _values.Sum * averageSquared * _values.Average + averageSquared * averageSquared) / _values.Count;
                    }
                case 5:
                    {
                        double averageSquared = _values.Average * _values.Average;
                        double averageQuadrupled = averageSquared * averageSquared;
                        return (_values.QuintileSum - 5.0 * _values.QuadraticSum * _values.Average + 10.0 * _values.CubicSum * averageSquared - 10.0 * _values.SquaredSum * averageSquared * _values.Average + 5.0 * _values.Sum * averageQuadrupled - averageQuadrupled * _values.Average) / _values.Count;
                    }
                default:
                    if (ThrowExceptions)
                    {
                        std::stringstream ss;
                        ss << "Statistics::Moment: moment of " << k << "th order not supported";
                        throw std::runtime_error(ss.str());
                    }
                    return NaN;
            }
        }
        if (ThrowExceptions)
        {
            std::stringstream ss;
            ss << "Statistics::Moment(): need at least one sample to compute moments";
            throw std::runtime_error(ss.str());
        }
        return NaN;
    }

    /**
     *  Compute the skewness of all samples.
     *
     *  @brief  Skewness is a measure of the symmetricity of the data set.
     *          Symmetric data set like normal distribution has a skewness of zero.
     *          If the the data set leans to the left or towards smaller values or
     *          right-hand tail is longer than the left-hand tail then
     *          the skewness is positive. If the data set leans to the right or
     *          towards bigger values or left-hand tail is longer than the right-hand
     *          tail then skewness is negative. For further information consult this article:
     *          https://www.spcforexcel.com/knowledge/basic-statistics/are-skewness-and-kurtosis-useful-statistics
     */
    double Skewness() const
    {
//printf("m2 %f, pow(m2, 1.5)=%f\n", moment2, std::pow(moment2, 1.5));
        //double moment2 = Moment(2);
        //if (moment2 > 0.0)
        //{
        //    double skewness = Moment(3) / moment2 / std::sqrt(moment2);
        //    return skewness;
        //}

        // Alternative formula for skewness from the above article:
        double n = _values.Count;
        double s = SD();
        if (n > 2.0 && s != 0.0)
        {
            double skewness = n / (n - 1) / (n - 2) * Moment(3) / s / s / s;
            return skewness;
        }
        if (ThrowExceptions)
        {
            std::stringstream ss;
            ss << "Statistics::Skewness(): need at least 3 samples and nonzero standard deviation to compute skewness";
            throw std::runtime_error(ss.str());
        }
        return NaN;
    }

    /**
     *  Compute the square of a value (x^2).
     *
     *  @param  x   Value to be squared.
     *
     *  @return Value squared.
     */
    static inline double Sqr(double x)
    {
        return x * x;
    }

    /**
     *  Compute the kurtosis of all samples.
     *
     *  @brief  Kurtosis is a measure of the combined weight of the data set tails
     *          (both start and endtails!) relative to the rest of the distribution.
     *          Negative kurtosis means that the tails of the data set are not much
     *          different from the rest. Positive curtosis means that the data set
     *          has clear tails compared to the rest.
     *
     *          Sometimes kurtosis is defined as "the peakedness" of the data set
     *          but this is not entirely accurate. For further information consult
     *          this article:
     *          https://www.spcforexcel.com/knowledge/basic-statistics/are-skewness-and-kurtosis-useful-statistics
     */
    double Kurtosis() const
    {
        //double moment2 = Moment(2);
        //if (moment2 != 0.0)
        //{
        //    //double kurtosis = Moment(4) / moment2 / moment2 - 3.0;
        //    //return kurtosis;
        //}

        // Alternative formula for kurtosis from the above article:
        double n = _values.Count;
        double s = SD();
        if (n > 3.0 && s != 0.0)
        {
            double kurtosis = n * (n + 1) / (n - 1) / (n - 2) / (n - 3) * (Moment(4) / s / s / s / s) - 3.0 * Sqr(n - 1) / (n - 2) / (n - 3);
            return kurtosis;
        }
        if (ThrowExceptions)
        {
            std::stringstream ss;
            ss << "Statistics::Skewness(): need at least 4 samples and nonzero standard deviation to compute kurtosis";
            throw std::runtime_error(ss.str());
        }
        return NaN;
    }

private:

    /**
     *  Compute mean as precisely as possible by binary summation when all samples are available.
     *  This is experimental code. Do not use in production.
     */
    double PreciseMean(std::size_t low, std::size_t high) const
    {
        if (low == high)
        {
            return Samples[low];
        }
        else
        {
            std::size_t middle = (high - low) / 2;
            if (middle == low)
            {
                return Samples[low];
            }
            else if (middle == high)
            {
                return Samples[high];
            }
            else
            {
                assert(low <= middle - 1);
                double mean1 = PreciseMean(low, middle - 1);
                assert(middle + 1 <= high);
                double mean2 = PreciseMean(middle + 1, high);
                double mean = (static_cast<double>(middle - 1 - low) * mean1 + static_cast<double>(high - middle - 1) * mean2) / static_cast<double>(high - low);
                return mean;
            }
        }
    }

public:

    /**
     *  Compute precise mean.
     *  This is experimental code. Do not use in production.
     */
    double PreciseMean() const
    {
        return PreciseMean(0, Samples.size() - 1);
    }
};

#endif /* __cplusplus */

#endif // __statistics_h__
