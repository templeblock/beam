#include "NoiseSuppressor.h"

namespace Beam{
	NoiseSuppressor::NoiseSuppressor(){

	}

	NoiseSuppressor::NoiseSuppressor(float frequency, int frame_size, float adaptive_tau, float suppress){
		init(frequency, frame_size, adaptive_tau, suppress);
	}

	NoiseSuppressor::~NoiseSuppressor(){

	}

	void NoiseSuppressor::init(float frequency, int frame_size, float adaptive_tau, float suppress){
		m_frame_duration = (float)frame_size / frequency;
		m_phase_adaptive_tau = adaptive_tau;
		m_speed_adaptive_tau = adaptive_tau * 2.f;

		m_noise_adaptive_tau = adaptive_tau;
		m_sampling_rate = frequency;

		set_suppress(suppress);

		m_phase_num_frames = 0;
		m_noise_num_frames = 0;
	}

	void NoiseSuppressor::phase_compensation(std::vector<std::complex<float> >& output){
		int bins = (int)output.size();
		//	Prepare some constants
		float phase_adaptive_ratio = (float)(m_frame_duration / m_phase_adaptive_tau);
		float speed_adaptive_ratio = (float)(m_frame_duration / m_speed_adaptive_tau);
		//  Check and create vectors
		if (m_phase_model.empty()){
			m_phase_model.assign(bins, std::complex<float>(0.f, 0.f));
		}
		if (m_phase_model_variance.empty()){
			m_phase_model_variance.assign(bins, 0.f);
		}
		if (m_phase_model_update.empty()){
			m_phase_model_update.assign(bins, std::complex<float>(0.f, 0.f));
			//  initialize it with one frame delay for each frequency
			for (int bin = 0; bin < bins; ++bin){
				float frequency = ((float)bin + 0.5f) * m_sampling_rate / 2.f / bins;
				float phase = (float)(TWO_PI * m_frame_duration * frequency);
				m_phase_model_update[bin] = std::complex<float>(cosf(phase), sinf(phase));
			}
		}
		if (m_phase_model_prev.empty()){
			m_phase_model_prev.assign(bins, std::complex<float>(0.f, 0.f));
		}
		//  Rotate the complex phase model
		for (int i = 0; i < bins; ++i){
			m_phase_model[i] *= m_phase_model_update[i];
		}
		//  Update the complex phase model and the speed
		for (int i = 0; i < bins; ++i){
			if (m_phase_num_frames == 0){
				m_phase_model[i] = output[i];
				continue;
			}
			if (m_phase_num_frames == 1){
				//	Average the first and second frame
				std::complex<float> element = output[i];
				std::complex<float> model = (m_phase_model[i] + element) * 0.5f;
				m_phase_model[i] = model;
				//	Compute the first variation value
				m_phase_model_variance[i] = std::norm(element - model);
				continue;
			}
			//  Update the complex phase model
			std::complex<float>& element = output[i];
			std::complex<float>& model = m_phase_model[i];
			std::complex<float> diff = output[i];
			diff -= model;
			float delta = Utils::norm_complex(diff);
			//  Avoid divide by 0
			float phase_var = m_phase_model_variance[i];
			float ratio = 0.f;
			if (phase_var != 0.f){
				ratio = expf(-delta / phase_var / 4.f);
			}
			float adapt = phase_adaptive_ratio * ratio;
			if (adapt < 1.f){
				diff *= adapt;
				model += diff;
			}
			else{
				model = element;
			}
			//  Update the variance
			m_phase_model_variance[i] = (1.f - phase_adaptive_ratio) * m_phase_model_variance[i] + phase_adaptive_ratio * delta;
			//  Update the complex phase model speed
			std::complex<float> current = output[i];
			std::complex<float> previous = m_phase_model_prev[i];
			std::complex<float>& speed = m_phase_model_update[i];
			Utils::normalize_complex(current);
			Utils::normalize_complex(previous);
			std::complex<float> speed_update = m_phase_model_update[i];
			speed_update *= previous;
			if (Utils::abs_complex(speed_update) == 0.f) continue;
			speed_update = current / speed_update;
			if (Utils::abs_complex(speed_update) == 0.f) continue;
			adapt = speed_adaptive_ratio * ratio;
			if (adapt < 1.f){
				speed_update *= adapt;
				speed *= speed_update;
			}
			else{
				speed *= speed_update;
			}
			Utils::normalize_complex(speed);
		}
		m_phase_model_prev.assign(output.begin(), output.end());
		//  Compensate the phase
		for (int i = 0; i < bins; ++i){
			output[i] -= m_phase_model[i];
		}
		++m_phase_num_frames;
	}

	void NoiseSuppressor::noise_compensation(std::vector<std::complex<float> >& output){
		int bins = (int)output.size();
		float noise_adaptive_ratio = (float)(m_frame_duration / m_noise_adaptive_tau);
		if (m_noise_model.empty()){
			m_noise_model.assign(bins, 0.f);
		}
		if (m_noise_model_variance.empty()){
			m_noise_model_variance.assign(bins, 0.f);
		}
		//	Suppress the stationary noise
		for (int i = 0; i < bins; ++i){
			float ratio = 1.f;
			float element = Utils::abs_complex(output[i]);
			float model = m_noise_model[i];
			if (m_noise_num_frames == 0){
				m_noise_model[i] = element;
				continue;
			}
			float delta = fabs(element - model);
			delta *= delta;
			if (m_noise_num_frames == 1){
				model = (model + element) / 2.f;
				m_noise_model[i] = model;
				m_noise_model_variance[i] = delta;
				continue;
			}
			if (m_noise_model_variance[i] != 0.f){
				ratio = expf(-delta / m_noise_model_variance[i] / 4.f);
			}
			float adapt = noise_adaptive_ratio * ratio;
			if (adapt < 1.f){
				model += (element - model) * adapt;
			}
			else{
				model = element;
			}
			m_noise_model[i] = model;
			//  Update the variance
			m_noise_model_variance[i] = (1.f - noise_adaptive_ratio) * m_noise_model_variance[i] + noise_adaptive_ratio * delta;
			//	Do the actual noise suppression
			float gain = 0.f;
			if (element > model){
				gain = (element * element - m_suppress * model * model) / (element * element);
			}
			else{
				gain = 1.f - m_suppress;
			}
			output[i] *= gain;
		}
		++m_noise_num_frames;
	}

	void NoiseSuppressor::frequency_shifting(std::vector<std::complex<float> >& output){
		int bins = (int)output.size();
		for (int i = 1; i < bins; ++i){
			std::complex<float> element_l = output[i - 1];
			std::complex<float> element_r = output[i];
			float mag_l = Utils::abs_complex(element_l);
			float arg_l = std::arg(element_l);
			float mag_r = Utils::abs_complex(element_r);
			float arg_r = std::arg(element_r);
			float mag = 0.8f * mag_l + 0.2f * mag_r;
			float arg = 0.8f * arg_l + 0.2f * arg_r;
			std::complex<float> element(mag * cosf(arg), mag * sinf(arg));
			output[i - 1] = element;
		}
		output.back() *= 0.f;
	}

	void NoiseSuppressor::set_suppress(float suppress){
		if (suppress <= 1.f){
			m_suppress = 0.f;
		}
		else{
			m_suppress = (1.f - 1.f / suppress);
		}
	}
}

