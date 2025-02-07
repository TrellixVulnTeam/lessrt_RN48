#pragma once

//Implementing a storage class to store directional BRF/radiance
//value for different directions

#if !defined(_FPARPRODUCT_H_)
#define _FPARPRODUCT_H_

#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include "photonRTUtils.h"
#include <mitsuba/core/spectrum.h>
#include <mitsuba/mitsuba.h>
#include <boost/algorithm/string.hpp>
#include <iomanip>      // std::setprecision

using namespace std;

MTS_NAMESPACE_BEGIN

class fPARProduct :public Object {
public:
	fPARProduct(string layerDefinition) {
		std::vector<std::string> tmp;
		boost::algorithm::split(tmp, layerDefinition, boost::is_any_of(":"));
		if (tmp.size() == 3) { //mode: from:step:to
			double from = atof(tmp[0].c_str());
			double step = atof(tmp[1].c_str());
			double to = atof(tmp[2].c_str());
			int i = 0;
			while (true) {
				double layerHeightLower = from + i * step;
				double layerheightUpper = from + (i + 1)*step;
				if (layerheightUpper > to) {
					break;
				}
				i++;
				m_layerLowerBounds.push_back(layerHeightLower);
				m_layerUpperBounds.push_back(layerheightUpper);
				m_totalFPAR.push_back(Spectrum(0.0));
			}
			m_mumberOfLayers = i;
		}
		m_numComponents = 0;

		for (int i = 0; i < 30; i++) {
			m_reprobs[i] = vector<int>();
			m_reprobs.at(i).push_back(0);
			m_reprobs.at(i).push_back(0);
		}
	}

	void serialize(Stream *stream) const {
		stream->writeInt(m_mumberOfLayers);
		stream->writeDoubleArray(m_layerLowerBounds.data(), m_mumberOfLayers);
		stream->writeDoubleArray(m_layerUpperBounds.data(), m_mumberOfLayers);
		stream->writeInt(m_numComponents);
		for (int i = 0; i < m_numComponents; i++)
			stream->writeString(m_components[i]);

		for (int i = 0; i < m_numComponents; i++) {
			string compName = m_components[i];
			for(int j=0;j<m_mumberOfLayers;j++)
				m_fPAR.at(compName)[j].serialize(stream);
		}

		//temp
		for (int i = 0; i < m_mumberOfLayers; i++)
			m_totalFPAR[i].serialize(stream);

		//prob
		for (int i = 0; i < 30; i++) {
			stream->writeIntArray(m_reprobs.at(i).data(), 2);
		}
	}
	void unserialize(Stream *stream) {
		m_mumberOfLayers = stream->readInt();
		m_layerLowerBounds.resize(m_mumberOfLayers);
		stream->readDoubleArray(m_layerLowerBounds.data(), m_mumberOfLayers);
		m_layerUpperBounds.resize(m_mumberOfLayers);
		stream->readDoubleArray(m_layerUpperBounds.data(), m_mumberOfLayers);
		m_numComponents = stream->readInt();
		m_components.resize(m_numComponents);
		for (int i = 0; i < m_numComponents; i++)
			m_components[i] = stream->readString();

		m_fPAR.clear();
		for (int i = 0; i < m_numComponents; i++) {
			string compName = m_components[i];
			m_fPAR[compName] = vector<Spectrum>();
			m_fPAR[compName].resize(m_mumberOfLayers);
			for (int j = 0; j<m_mumberOfLayers; j++)
				m_fPAR.at(compName)[j] = Spectrum(stream);
		}

		//temp
		for (int i = 0; i < m_mumberOfLayers; i++)
			m_totalFPAR[i]=Spectrum(stream);

		//prob
		for (int i = 0; i < 30; i++) {
			m_reprobs[i] = vector<int>();
			m_reprobs.at(i).resize(2);
			stream->readIntArray(m_reprobs.at(i).data(), 2);
		}
	}

	double broadbandEnergy(Spectrum energy) {
		if (SPECTRUM_SAMPLES == 1)
			return energy[0];

		double total = 0.0;
		for (int j = 0; j < SPECTRUM_SAMPLES-1; j++) {
			total += (energy[j] + energy[j + 1])*(m_wavelengths[j + 1] - m_wavelengths[j])*0.5;
		}
		return total;
	}

	void develop(double scale = 1.0) {
		if (m_destnationFile != "") {
			Spectrum totalIncidentW = scale * 1 / (m_scenBoundPlaneSize.x * m_scenBoundPlaneSize.y)*(m_virtualBoundXZSize.x*m_virtualBoundXZSize.y)*m_verticalIrradiance;
			ofstream out(m_destnationFile);
			out << "**FPAR**" << endl;
			out << "layer_bottom  layer_upper  TfPAR ";
			for (int i = 0; i < m_numComponents; i++)
				out << m_components[i] << " ";
			out << endl;
			for (int i = 0; i < m_mumberOfLayers; i++) {
				out <<std::fixed << std::setprecision(4) << setw(8) << m_layerLowerBounds[i] << " " << setw(8) << m_layerUpperBounds[i] << " ";
				out << std::fixed << std::setprecision(4) << setw(8) << broadbandEnergy(scale*m_totalFPAR[i]) / broadbandEnergy(totalIncidentW) << " ";
				for (int j = 0; j < m_numComponents; j++) {
					string compName = m_components[j];
					double fpar = broadbandEnergy(scale*m_fPAR.at(compName)[i]) / broadbandEnergy(totalIncidentW);
					out << std::fixed << std::setprecision(4)<< setw(8) << fpar<<" ";
				}
				out << endl;
			}

			//Absorption for each band
			out <<endl<< "**Absorption for each band**"<<endl;
			out << " - Total Absorption" << endl;
			out << "layer_bottom  layer_upper  Absorption_For_Each_Band..."<<endl;
			for (int j = 0; j < m_mumberOfLayers; j++) {
				out << std::fixed << std::setprecision(4) << setw(8) << m_layerLowerBounds[j] << " " << setw(8) << m_layerUpperBounds[j] << " ";
				for (int k = 0; k < SPECTRUM_SAMPLES; k++) {
					out << std::fixed << std::setprecision(4) << setw(8) << scale * m_totalFPAR[j][k] / totalIncidentW[k] << " ";
				}
				out << endl;
			}
			for (int i = 0; i < m_numComponents; i++) {
				out <<endl<< " - Absorption of "<< m_components[i] << endl;
				out << "layer_bottom  layer_upper  Absorption_For_Each_Band..."<<endl;
				for (int j = 0; j < m_mumberOfLayers; j++) {
					out << std::fixed << std::setprecision(4) << setw(8) << m_layerLowerBounds[j] << " " << setw(8) << m_layerUpperBounds[j] << " ";
					string compName = m_components[i];
					Spectrum fpar = scale*m_fPAR.at(compName)[j] / totalIncidentW;
					for (int k = 0; k < SPECTRUM_SAMPLES; k++) {
						out << std::fixed << std::setprecision(4) << setw(8) << fpar[k] << " ";
					}
					out << endl;
				}
			}
			out.close();

		}

		//prob
		if (m_destnationProbFile != "") {
			ofstream out(m_destnationProbFile);
			out << "**Spectral invariant (probability)**" << endl;
			out << " - Total probability" << endl;
			int totalPhotons = 0;
			int collisionPhotons = 0;
			for (int i = 0; i < 30; i++) {
				totalPhotons += m_reprobs.at(i)[0];
				collisionPhotons += m_reprobs.at(i)[1];
			}
			out << std::fixed << std::setprecision(4) << setw(8) << (double)collisionPhotons / (double(totalPhotons)) << endl;
			out << endl<< " - Probability for each scattering order" << endl;
			out << "Scattering_order Total_photons Intersected_photons" << endl;
			for (int i = 0; i < 30; i++) {
				out << std::fixed << setw(2) << (i + 1) <<" ";
				out << std::fixed << std::setprecision(10) << setw(8)<< m_reprobs.at(i)[0] << " ";
				out << std::fixed << std::setprecision(10) << setw(8)<< m_reprobs.at(i)[1] << endl;
			}
			out.close();
		}

	}

	void clear() {
		m_verticalIrradiance = Spectrum(0.0);
		m_fPAR.clear();

		m_numComponents = 0;
		m_components.clear();

		//
		//temp

		m_totalFPAR.clear();
		for (int i = 0; i < m_mumberOfLayers; i++)
			m_totalFPAR.push_back(Spectrum(0.0));

		//prob
		for (int i = 0; i < 30; i++) {
			m_reprobs.at(i) = vector<int>();
			m_reprobs.at(i).push_back(0);
			m_reprobs.at(i).push_back(0);
		}
	}

	//merge another directionalBRF in current one
	void merge(const fPARProduct* fPARs) {
		m_verticalIrradiance += fPARs->m_verticalIrradiance;
		for (int i = 0; i < fPARs->m_numComponents; i++) {
			string compName = fPARs->m_components[i];
			vector<Spectrum> compLayerFpar = fPARs->m_fPAR.at(compName);
			if (m_fPAR.count(compName) == 0) {//δ����
				m_fPAR[compName] = fPARs->m_fPAR.at(compName);
				m_numComponents++;
				m_components.push_back(compName);
			}
			else {
				for (int j = 0; j < m_mumberOfLayers; j++)
					m_fPAR.at(compName)[j] += fPARs->m_fPAR.at(compName)[j];
			}
		}

		//tmp
		for (int i = 0; i < m_mumberOfLayers; i++) {
			m_totalFPAR[i] += fPARs->m_totalFPAR[i];
		}

		//prob
		for (int i = 0; i < 30; i++) {
			m_reprobs.at(i)[0] += fPARs->m_reprobs.at(i)[0];
			m_reprobs.at(i)[1] += fPARs->m_reprobs.at(i)[1];
		}
	}

	//Collect total irradiance at the top of the virtual plane for BRF calculation.
	void putIrradiance(Spectrum value) {
		m_verticalIrradiance += value;
	}

	void put(int depth,const Intersection &its, Spectrum absorbedEnergy) {
		Point p = its.p;
		string compName = its.shape->getID();
		double step = m_layerUpperBounds[0] - m_layerLowerBounds[0];
		int index = (int)((p.y - m_layerLowerBounds[0]) / step);
		if (index >= 0 && index <= m_mumberOfLayers - 1) {

			//tmp
			m_totalFPAR[index] += absorbedEnergy;

			if (m_fPAR.count(compName) == 0) {
				m_numComponents++;
				m_components.push_back(compName);
				m_fPAR[compName].resize(m_mumberOfLayers);
				for (int j = 0; j<m_mumberOfLayers; j++)
					m_fPAR.at(compName)[j] = Spectrum(0.0);	
			}
			m_fPAR.at(compName)[index] += absorbedEnergy;
		}
	}

	void putReProb(int depth, const Intersection &its, Spectrum absorbedEnergy, int previousStatus) {
		if (depth >= 2 && depth <= 31 && (previousStatus == 2)) {
			m_reprobs.at(depth - 2)[0]++;
			if (its.t != std::numeric_limits<Float>::infinity()) {
				string compName = its.shape->getID();
				if (compName != "terrain") {
					m_reprobs.at(depth - 2)[1]++;
				}
			}
		}
	}

	void setDestinationFile(string destinationFile) {
		m_destnationFile = destinationFile;
	}
	void setDestnationProbFile(string destnationProbFile) {
		m_destnationProbFile = destnationProbFile;
	}

	void setSceneBoundPlaneSize(Vector2 size) {
		this->m_scenBoundPlaneSize = size;
	}

	void setVirtualBoundXZSize(Vector2 size) {
		this->m_virtualBoundXZSize = size;
	}
	void setWavelengths(Spectrum spectrum) {
		this->m_wavelengths = spectrum;
	}

public:
	vector<double> m_layerLowerBounds;
	vector<double> m_layerUpperBounds;
	int m_numComponents;// Number of components
	vector<string> m_components;
	map<string, vector<Spectrum> > m_fPAR;
	int m_mumberOfLayers;
	vector<Spectrum> m_totalFPAR;

	//re-collision probablity
	map<int, vector<int>> m_reprobs;
	string m_destnationProbFile;

	//no need to serilize
	string  m_destnationFile;//The path to store the fpar products
	Vector2 m_scenBoundPlaneSize; //used to calculate totcal incident energy w/m2
	Spectrum m_verticalIrradiance; // total energy incident on the top of the scene
	Vector2 m_virtualBoundXZSize; //only compute the energy which has been incident on the virtual plane
	Spectrum m_wavelengths;
};


MTS_NAMESPACE_END
#endif