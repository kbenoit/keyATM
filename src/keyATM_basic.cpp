#include "keyATM_basic.h"

using namespace Eigen;
using namespace Rcpp;
using namespace std;


keyATMbasic::keyATMbasic(List model_, const int iter_, const int output_per_) :
	keyATMbase(model_, iter_, output_per_) // pass to parent!
{
	// Constructor
	read_data();
	initialize();
	iteration();
}


void keyATMbasic::read_data_specific()
{
	alpha = Rcpp::as<Eigen::VectorXd>(nv_alpha);
}


void keyATMbasic::initialize_specific()
{
	// No additional initialization
}

void keyATMbasic::iteration_single()
{ // Single iteration

	std::vector<int> doc_indexes = sampler::shuffled_indexes(num_doc); // shuffle

	for (int ii = 0; ii < num_doc; ii++){
		int doc_id_ = doc_indexes[ii];
		IntegerVector doc_x = X[doc_id_], doc_z = Z[doc_id_], doc_w = W[doc_id_];
		
		std::vector<int> token_indexes = sampler::shuffled_indexes(doc_z.size()); //shuffle
		
		// Iterate each word in the document
		for (int jj = 0; jj < doc_z.size(); jj++){
			int w_position = token_indexes[jj];
			int x_ = doc_x[w_position], z_ = doc_z[w_position], w_ = doc_w[w_position];
		
			int new_z = sample_z(alpha, z_, x_, w_, doc_id_);
			doc_z[w_position] = new_z;
		
	
			z_ = doc_z[w_position]; // use updated z
			int new_x = sample_x(alpha, z_, x_, w_, doc_id_);
			doc_x[w_position] = new_x;
		}
		
		Z[doc_id_] = doc_z;
		X[doc_id_] = doc_x;
	}
	sample_parameters();

}

void keyATMbasic::sample_parameters()
{
	sample_alpha();
}


void keyATMbasic::sample_alpha()
{

  double start, end, previous_p, new_p, newlikelihood, slice_;
  VectorXd keep_current_param = alpha;
  std::vector<int> topic_ids = sampler::shuffled_indexes(num_topics);
	double store_loglik = alpha_loglik();
	double newalphallk = 0.0;

  for(int i = 0; i < num_topics; i++){
    int k = topic_ids[i];
    start = min_v / (1.0 + min_v); // shrinkp
    end = 1.0;
    // end = shrinkp(max_v);
		previous_p = alpha(k) / (1.0 + alpha(k)); // shrinkp
    slice_ = store_loglik - 2.0 * log(1.0 - previous_p) 
						+ log(unif_rand()); // <-- using R random uniform

    for (int shrink_time = 0; shrink_time < max_shrink_time; shrink_time++){
      new_p = sampler::slice_uniform(start, end); // <-- using R function above
      alpha(k) = new_p / (1.0 - new_p); // expandp

			newalphallk = alpha_loglik();
      newlikelihood = newalphallk - 2.0 * log(1.0 - new_p);

      if (slice_ < newlikelihood){
				store_loglik = newalphallk;
        break;
      } else if (previous_p < new_p){
        end = new_p;
      } else if (new_p < previous_p){
        start = new_p;
      } else {
				Rcerr << "Something goes wrong in sample_alpha()" << std::endl;
        alpha(k) = keep_current_param(k);
				break;
      }
    }
  }

	model["alpha"] = alpha;

	// Store alpha
	NumericVector alpha_rvec = alpha_reformat(alpha, num_topics);
	List alpha_iter = model["alpha_iter"];
	alpha_iter.push_back(alpha_rvec);
	model["alpha_iter"] = alpha_iter;
}


double keyATMbasic::alpha_loglik()
{
  double loglik = 0.0;
  double fixed_part = 0.0;
  MatrixXd ndk_a = n_dk.rowwise() + alpha.transpose(); // Use Eigen Broadcasting


  fixed_part += lgamma(alpha.sum()); // first term numerator
  for(int k = 0; k < num_topics; k++){
    fixed_part -= lgamma(alpha(k)); // first term denominator
    // Add prior
		if(k < k_seeded){
			loglik += gammapdfln(alpha(k), eta_1, eta_2);
		}else{
			loglik += gammapdfln(alpha(k), eta_1_regular, eta_2_regular);
		}

  }
  for(int d = 0; d < num_doc; d++){
    loglik += fixed_part;
    // second term numerator
    for(int k = 0; k < num_topics; k++){
      loglik += lgamma(ndk_a(d,k));
    }
    // second term denominator
    loglik -= lgamma(ndk_a.row(d).sum());

  }
  return loglik;
}


double keyATMbasic::loglik_total()
{
  double loglik = 0.0;
  for (int k = 0; k < num_topics; k++){
    for (int v = 0; v < num_vocab; v++){ // word
      loglik += lgamma(beta + (double)n_x0_kv(k, v) ) - lgamma(beta);
      loglik += lgamma(beta_s + (double)n_x1_kv(k, v) ) - lgamma(beta_s);
    }
    // word normalization
    loglik += lgamma( beta * (double)num_vocab ) - lgamma(beta * (double)num_vocab + (double)n_x0_k(k) );
    loglik += lgamma( beta_s * (double)num_vocab ) - lgamma(beta_s * (double)num_vocab + (double)n_x1_k(k) );
    // x
    loglik += lgamma( (double)n_x0_k(k) + gamma_2 ) - lgamma((double)n_x1_k(k) + gamma_1 + (double)n_x0_k(k) + gamma_2)
      + lgamma( (double)n_x1_k(k) + gamma_1 ) ;

		// Rcout << (double)n_x0_k(k) << " / " << (double)n_x1_k(k) << std::endl; // debug

    // x normalization
    loglik += lgamma(gamma_1 + gamma_2) - lgamma(gamma_1) - lgamma(gamma_2);
  }
  // z
  for (int d = 0; d < num_doc; d++){
    loglik += lgamma( alpha.sum() ) - lgamma( n_dk.row(d).sum() + alpha.sum() );
    for (int k = 0; k < num_topics; k++){
      loglik += lgamma( n_dk(d,k) + alpha(k) ) - lgamma( alpha(k) );
    }
  }
  return loglik;
}
