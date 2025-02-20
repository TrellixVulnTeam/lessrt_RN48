/*
	This file is part of Mitsuba, a physically based rendering system.

	Copyright (c) 2007-2014 by Wenzel Jakob and others.

	Mitsuba is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License Version 3
	as published by the Free Software Foundation.

	Mitsuba is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/render/scene.h>
#include "maxexp.h"

MTS_NAMESPACE_BEGIN


	class VegMedium : public Medium {
	public:
		/**
		 * This class supports the following sampling strategies for choosing
		 * a suitable scattering location when sampling the RTE
		 */
		enum ESamplingStrategy {
			EBalance,  /// Exponential distrib.; pick a random channel each time
			ESingle,   /// Exponential distrib.; pick a specified channel
			EManual,   /// Exponential distrib.; manually specify the falloff
			EMaximum   /// Maximum-of-exponential distribution
		};

		VegMedium(const Properties &props)
			: Medium(props), m_samplingDensity(0.0f), m_maxExpDist(NULL) {
			std::string strategy = props.getString("strategy", "balance");

			Float leafAreaDensity = props.getFloat("leafAreaDensity", 1.2);
			m_sigmaT = Spectrum(0.5 * leafAreaDensity);

			Spectrum singleScatteringAlbedo = props.getSpectrum("singleScatteringAlbedo", Spectrum(0.5));
			//std::cout << "INFO:" << singleScatteringAlbedo.toString() << std::endl;
			m_sigmaS = m_sigmaT * singleScatteringAlbedo;
			m_sigmaA = m_sigmaT - m_sigmaS;

			/**
			 * The goal of the medium sampling weight is to be able to
			 * sample medium intarctions according to
			 *    sigma_s(t) * tau(0 <-> t)
			 * as opposed to
			 *    sigma_t(t) * tau(0 <-> t)
			 * See the separate writeup for more details.
			 */
			m_mediumSamplingWeight = props.getFloat("mediumSamplingWeight", -1);
			if (m_mediumSamplingWeight == -1) {
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
					/// Record the highest albedo values across channels
					Float albedo = m_sigmaS[i] / m_sigmaT[i];
					if (albedo > m_mediumSamplingWeight && m_sigmaT[i] != 0)
						m_mediumSamplingWeight = albedo;
				}
				if (m_mediumSamplingWeight > 0) {
					/* The medium scatters some light -> place at least half
					   of the samples in it, otherwise we will render lots
					   of spatially varying noise where one pixel has a
					   medium interaction and the neighbors don't */
					m_mediumSamplingWeight = std::max(m_mediumSamplingWeight,
						(Float) 0.5f);
				}
			}

			if (strategy == "balance") {
				m_strategy = EBalance;
			}
			else if (strategy == "single") {
				m_strategy = ESingle;

				/* By default, choose the lowest-variance channel
				   (the one with the smallest sigma_t, that is) */
				int channel = 0;
				Float smallest = std::numeric_limits<Float>::infinity();
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
					if (m_sigmaT[i] < smallest) {
						smallest = m_sigmaT[i];
						channel = i;
					}
				}

				channel = props.getInteger("channel", channel);
				Assert(channel >= 0 && channel < SPECTRUM_SAMPLES);
				m_samplingDensity = m_sigmaT[channel];

				if (props.getBoolean("monochromatic", false)) {
					/* Optionally turn this into a monochromatic medium
					   based on the chosen color channel. This is useful
					   when the whole scene is rendered once per channel
					   and then recombined to create a color image */
					m_sigmaA = Spectrum(m_sigmaA[channel]);
					m_sigmaS = Spectrum(m_sigmaS[channel]);
					m_sigmaT = m_sigmaA + m_sigmaS;
				}
			}
			else if (strategy == "maximum") {
				m_strategy = EMaximum;
				std::vector<Float> coeffs(SPECTRUM_SAMPLES);
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
					coeffs[i] = m_sigmaT[i];
				m_maxExpDist = new MaxExpDist(coeffs);
			}
			else if (strategy == "manual") {
				m_strategy = EManual;
				m_samplingDensity = props.getFloat("samplingDensity");
			}
			else {
				Log(EError, "Specified an unknown sampling strategy");
			}
		}

		VegMedium(Stream *stream, InstanceManager *manager)
			: Medium(stream, manager), m_maxExpDist(NULL) {
			m_strategy = (ESamplingStrategy)stream->readInt();
			m_samplingDensity = stream->readFloat();
			m_mediumSamplingWeight = stream->readFloat();

			if (m_strategy == EMaximum) {
				std::vector<Float> coeffs(SPECTRUM_SAMPLES);
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
					coeffs[i] = m_sigmaT[i];
				m_maxExpDist = new MaxExpDist(coeffs);
			}

			configure();
		}

		virtual ~VegMedium() {
			if (m_maxExpDist)
				delete m_maxExpDist;
		}

		void configure() {
			Medium::configure();
			m_albedo = 0;
			for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
				if (m_sigmaT[i] != 0)
					m_albedo = std::max(m_albedo, m_sigmaS[i] / m_sigmaT[i]);
			}
		}

		void serialize(Stream *stream, InstanceManager *manager) const {
			Medium::serialize(stream, manager);
			stream->writeInt(m_strategy);
			stream->writeFloat(m_samplingDensity);
			stream->writeFloat(m_mediumSamplingWeight);
		}

		Spectrum evalTransmittance(const Ray &ray, Sampler *) const {
			Float negLength = ray.mint - ray.maxt;
			Spectrum transmittance;
			for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
				transmittance[i] = m_sigmaT[i] != 0
				? math::fastexp(m_sigmaT[i] * negLength) : (Float) 1.0f;
			return transmittance;
		}

		bool sampleDistance(const Ray &ray, MediumSamplingRecord &mRec,
			Sampler *sampler) const {
			Float rand = sampler->next1D(), sampledDistance;
			Float samplingDensity = m_samplingDensity;

			if (rand < m_mediumSamplingWeight) {
				rand /= m_mediumSamplingWeight; //albedo
				if (m_strategy != EMaximum) {
					/* Choose the sampling density to be used */
					if (m_strategy == EBalance) {
						int channel = std::min((int)(sampler->next1D()
							* SPECTRUM_SAMPLES), SPECTRUM_SAMPLES - 1);
						samplingDensity = m_sigmaT[channel];
					}
					sampledDistance = -math::fastlog(1 - rand) / samplingDensity;
				}
				else {
					sampledDistance = m_maxExpDist->sample(1 - rand, mRec.pdfSuccess);
				}
			}
			else {
				/* Don't generate a medium interaction */
				sampledDistance = std::numeric_limits<Float>::infinity();
			}
			Float distSurf = ray.maxt - ray.mint;
			bool success = true;
			if (sampledDistance < distSurf) {
				mRec.t = sampledDistance + ray.mint;
				mRec.p = ray(mRec.t);
				mRec.sigmaA = m_sigmaA;
				mRec.sigmaS = m_sigmaS;
				mRec.time = ray.time;
				mRec.medium = this;

				/* Fail if there is no forward progress
				   (e.g. due to roundoff errors) */
				if (mRec.p == ray.o)
					success = false;
			}
			else {
				sampledDistance = distSurf;
				success = false;
			}

			switch (m_strategy) {
			case EMaximum:
				mRec.pdfFailure = 1 - m_maxExpDist->cdf(sampledDistance);
				break;

			case EBalance:
				mRec.pdfFailure = 0;
				mRec.pdfSuccess = 0;
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
					Float tmp = math::fastexp(-m_sigmaT[i] * sampledDistance);
					mRec.pdfFailure += tmp;
					mRec.pdfSuccess += m_sigmaT[i] * tmp;
				}
				mRec.pdfFailure /= SPECTRUM_SAMPLES;
				mRec.pdfSuccess /= SPECTRUM_SAMPLES;
				break;

			case ESingle:
			case EManual:
				mRec.pdfFailure = math::fastexp(-samplingDensity * sampledDistance);
				mRec.pdfSuccess = samplingDensity * mRec.pdfFailure;
				break;

			default:
				Log(EError, "Unknown sampling strategy!");
			}

			mRec.transmittance = (m_sigmaT * (-sampledDistance)).exp();
			mRec.pdfSuccessRev = mRec.pdfSuccess = mRec.pdfSuccess * m_mediumSamplingWeight;
			mRec.pdfFailure = m_mediumSamplingWeight * mRec.pdfFailure + (1 - m_mediumSamplingWeight);
			mRec.medium = this;
			if (mRec.transmittance.max() < 1e-20)
				mRec.transmittance = Spectrum(0.0f);

			return success;
		}

		void eval(const Ray &ray, MediumSamplingRecord &mRec) const {
			Float distance = ray.maxt - ray.mint;
			switch (m_strategy) {
			case EManual:
			case ESingle: {
				Float temp = math::fastexp(-m_samplingDensity * distance);
				mRec.pdfSuccess = m_samplingDensity * temp;
				mRec.pdfFailure = temp;
			}
						  break;

			case EBalance: {
				mRec.pdfSuccess = 0;
				mRec.pdfFailure = 0;
				for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
					Float temp = math::fastexp(-m_sigmaT[i] * distance);
					mRec.pdfSuccess += m_sigmaT[i] * temp;
					mRec.pdfFailure += temp;
				}
				mRec.pdfSuccess /= SPECTRUM_SAMPLES;
				mRec.pdfFailure /= SPECTRUM_SAMPLES;
			}
						   break;

			case EMaximum:
				mRec.pdfSuccess = m_maxExpDist->pdf(distance);
				mRec.pdfFailure = 1 - m_maxExpDist->cdf(distance);
				break;

			default:
				Log(EError, "Unknown sampling strategy!");
			}

			mRec.transmittance = (m_sigmaT * (-distance)).exp();
			mRec.pdfSuccess = mRec.pdfSuccessRev = mRec.pdfSuccess * m_mediumSamplingWeight;
			mRec.pdfFailure = mRec.pdfFailure * m_mediumSamplingWeight + (1 - m_mediumSamplingWeight);
			mRec.sigmaA = m_sigmaA;
			mRec.sigmaS = m_sigmaS;
			mRec.time = ray.time;
			mRec.medium = this;
			if (mRec.transmittance.max() < 1e-20)
				mRec.transmittance = Spectrum(0.0f);
		}

		bool isHomogeneous() const {
			return true;
		}

		std::string toString() const {
			std::ostringstream oss;
			oss << "VegMedium[" << endl
				<< "  sigmaA = " << m_sigmaA.toString() << "," << endl
				<< "  sigmaS = " << m_sigmaS.toString() << "," << endl
				<< "  sigmaT = " << m_sigmaT.toString() << "," << endl
				<< "  mediumSamplingWeight = " << m_mediumSamplingWeight << "," << endl
				<< "  samplingDensity = " << m_samplingDensity << "," << endl
				<< "  strategy = ";

			switch (m_strategy) {
			case ESingle: oss << "single," << endl; break;
			case EManual: oss << "manual," << endl; break;
			case EBalance: oss << "balance," << endl; break;
			case EMaximum: oss << "maximum," << endl; break;
			}

			oss << "  phase = " << indent(m_phaseFunction.toString()) << endl
				<< "]";
			return oss.str();
		}

		MTS_DECLARE_CLASS()
	private:
		Float m_samplingDensity, m_mediumSamplingWeight;
		ESamplingStrategy m_strategy;
		MaxExpDist *m_maxExpDist;
		Float m_albedo;
};

MTS_IMPLEMENT_CLASS_S(VegMedium, false, Medium)
MTS_EXPORT_PLUGIN(VegMedium, "Vegetation medium");
MTS_NAMESPACE_END
