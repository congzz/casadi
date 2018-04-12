// NOLINT(legal/copyright)

// C-REPLACE "fmin" "casadi_fmin"
// C-REPLACE "fmax" "casadi_fmax"
// C-REPLACE "std::max" "casadi_max"

// SYMBOL "qp_prob"
template<typename T1>
struct casadi_qp_prob {
  // Dimensions
  casadi_int nx, na, nz;
  // Smallest nonzero number
  T1 dmin;
  // Infinity
  T1 inf;
  // Dual to primal error
  T1 du_to_pr;
  // Print iterations
  int print_iter;
  // Sparsity patterns
  const casadi_int *sp_a, *sp_h, *sp_at, *sp_kkt;
  // Symbolic QR factorization
  const casadi_int *prinv, *pc, *sp_v, *sp_r;
};
// C-REPLACE "casadi_qp_prob<T1>" "struct casadi_qp_prob"

// SYMBOL "qp_work"
template<typename T1>
void casadi_qp_work(casadi_qp_prob<T1>* p, casadi_int* sz_iw, casadi_int* sz_w) {
  // Local variables
  casadi_int nnz_a, nnz_kkt, nnz_v, nnz_r;
  // Get matrix number of nonzeros
  nnz_a = p->sp_a[2+p->sp_a[1]];
  nnz_kkt = p->sp_kkt[2+p->sp_kkt[1]];
  nnz_v = p->sp_v[2+p->sp_v[1]];
  nnz_r = p->sp_r[2+p->sp_r[1]];
  // Reset sz_w, sz_iw
  *sz_w = *sz_iw = 0;
  // Temporary work vectors
  *sz_w = std::max(*sz_w, p->nz); // casadi_project, tau memory
  *sz_iw = std::max(*sz_iw, p->nz); // casadi_trans, tau type, allzero
  *sz_w = std::max(*sz_w, 2*p->nz); // casadi_qr
  // Persistent work vectors
  *sz_w += nnz_kkt; // kkt
  *sz_w += p->nz; // z=[xk,gk]
  *sz_w += p->nz; // lbz
  *sz_w += p->nz; // ubz
  *sz_w += p->nz; // lam
  *sz_w += nnz_a; // trans(a)
  *sz_w += p->nz; // dz
  *sz_w += p->nz; // dlam
  *sz_w += p->nx; // infeas
  *sz_w += p->nx; // tinfeas
  *sz_iw += p->nz; // neverzero
  *sz_iw += p->nz; // neverupper
  *sz_iw += p->nz; // neverlower
  *sz_w += std::max(nnz_v+nnz_r, nnz_kkt); // [v,r] or trans(kkt)
  *sz_w += p->nz; // beta
}

// SYMBOL "qp_data"
template<typename T1>
struct casadi_qp_data {
  // Problem structure
  const casadi_qp_prob<T1>* prob;
  // Cost
  T1 f;
  // QP data
  const T1 *nz_a, *nz_h, *g;
  // Vectors
  T1 *z, *lbz, *ubz, *infeas, *tinfeas, *lam, *w, *dz, *dlam;
  casadi_int *iw, *neverzero, *neverlower, *neverupper;
  // Numeric QR factorization
  T1 *nz_at, *nz_kkt, *beta, *nz_v, *nz_r;
  // Message buffer
  char msg[40];
  // Stepsize
  T1 tau;
  // Singularity
  casadi_int sing;
  // Smallest diagonal value for the QR factorization
  T1 mina;
  casadi_int imina;
  // Primal and dual error, corresponding index
  T1 pr, du;
  casadi_int ipr, idu;
};
// C-REPLACE "casadi_qp_data<T1>" "struct casadi_qp_data"

// SYMBOL "qp_init"
template<typename T1>
void casadi_qp_init(casadi_qp_data<T1>* d, casadi_int* iw, T1* w) {
  // Local variables
  casadi_int nnz_a, nnz_kkt, nnz_v, nnz_r;
  const casadi_qp_prob<T1>* p = d->prob;
  // Get matrix number of nonzeros
  nnz_a = p->sp_a[2+p->sp_a[1]];
  nnz_kkt = p->sp_kkt[2+p->sp_kkt[1]];
  nnz_v = p->sp_v[2+p->sp_v[1]];
  nnz_r = p->sp_r[2+p->sp_r[1]];
  d->nz_kkt = w; w += nnz_kkt;
  d->z = w; w += p->nz;
  d->lbz = w; w += p->nz;
  d->ubz = w; w += p->nz;
  d->lam = w; w += p->nz;
  d->dz = w; w += p->nz;
  d->dlam = w; w += p->nz;
  d->nz_v = w; w += std::max(nnz_v+nnz_r, nnz_kkt);
  d->nz_r = d->nz_v + nnz_v;
  d->beta = w; w += p->nz;
  d->nz_at = w; w += nnz_a;
  d->infeas = w; w += p->nx;
  d->tinfeas = w; w += p->nx;
  d->neverzero = iw; iw += p->nz;
  d->neverupper = iw; iw += p->nz;
  d->neverlower = iw; iw += p->nz;
  d->w = w;
  d->iw = iw;
}

// SYMBOL "qp_reset"
template<typename T1>
int casadi_qp_reset(casadi_qp_data<T1>* d) {
  // Local variables
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  // Reset variables corresponding to previous iteration
  d->msg[0] = '\0';
  d->tau = 0.;
  d->sing = 0;
  // Correct lam if needed, determine permitted signs
  for (i=0; i<p->nz; ++i) {
    // Permitted signs for lam
    d->neverzero[i] = d->lbz[i]==d->ubz[i];
    d->neverupper[i] = isinf(d->ubz[i]);
    d->neverlower[i] = isinf(d->lbz[i]);
    if (d->neverzero[i] && d->neverupper[i] && d->neverlower[i]) return 1;
    // Correct initial active set if required
    if (d->neverzero[i] && d->lam[i]==0.) {
      d->lam[i] = d->neverupper[i]
                || d->z[i]-d->lbz[i] <= d->ubz[i]-d->z[i] ? -p->dmin : p->dmin;
    } else if (d->neverupper[i] && d->lam[i]>0.) {
      d->lam[i] = d->neverzero[i] ? -p->dmin : 0.;
    } else if (d->neverlower[i] && d->lam[i]<0.) {
      d->lam[i] = d->neverzero[i] ? p->dmin : 0.;
    }
  }
  // Transpose A
  casadi_trans(d->nz_a, p->sp_a, d->nz_at, p->sp_at, d->iw);
  return 0;
}

// SYMBOL "qp_pr"
template<typename T1>
void casadi_qp_pr(casadi_qp_data<T1>* d) {
  // Calculate largest constraint violation
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  d->pr = 0;
  d->ipr = -1;
  for (i=0; i<p->nz; ++i) {
    if (d->z[i] > d->ubz[i]+d->pr) {
      d->pr = d->z[i]-d->ubz[i];
      d->ipr = i;
    } else if (d->z[i] < d->lbz[i]-d->pr) {
      d->pr = d->lbz[i]-d->z[i];
      d->ipr = i;
    }
  }
}

// SYMBOL "qp_du"
template<typename T1>
void casadi_qp_du(casadi_qp_data<T1>* d) {
  // Calculate largest constraint violation
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  d->du = 0;
  d->idu = -1;
  for (i=0; i<p->nx; ++i) {
    if (d->infeas[i] > d->du) {
      d->du = d->infeas[i];
      d->idu = i;
    } else if (d->infeas[i] < -d->du) {
      d->du = -d->infeas[i];
      d->idu = i;
    }
  }
}

// SYMBOL "qp_log"
template<typename T1>
void casadi_qp_log(casadi_qp_data<T1>* d, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(d->msg, sizeof(d->msg), fmt, args);
  va_end(args);
}

// SYMBOL "qp_pr_index"
template<typename T1>
casadi_int casadi_qp_pr_index(casadi_qp_data<T1>* d, casadi_int* sign) {
  // Try to improve primal feasibility by adding a constraint
  if (d->lam[d->ipr]==0.) {
    // Add the most violating constraint
    *sign = d->z[d->ipr]<d->lbz[d->ipr] ? -1 : 1;
    casadi_qp_log(d, "Added %lld to reduce |pr|", d->ipr);
    return d->ipr;
  }
  return -1;
}

// SYMBOL "qp_du_check"
template<typename T1>
T1 casadi_qp_du_check(casadi_qp_data<T1>* d, casadi_int i) {
  // Local variables
  casadi_int k;
  T1 new_du;
  const casadi_int *at_colind, *at_row;
  const casadi_qp_prob<T1>* p = d->prob;
  // AT sparsity
  at_colind = p->sp_at + 2;
  at_row = at_colind + p->na + 1;
  // Maximum infeasibility from setting from setting lam[i]=0
  if (i<p->nx) {
    new_du = fabs(d->infeas[i]-d->lam[i]);
  } else {
    new_du = 0.;
    for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
      new_du = fmax(new_du, fabs(d->infeas[at_row[k]]-d->nz_at[k]*d->lam[i]));
    }
  }
  return new_du;
}

// SYMBOL "qp_du_index"
template<typename T1>
casadi_int casadi_qp_du_index(casadi_qp_data<T1>* d, casadi_int* sign) {
  // Try to improve dual feasibility by removing a constraint
  // Local variables
  casadi_int best_ind, i;
  T1 best_w;
  const casadi_qp_prob<T1>* p = d->prob;
  // We need to increase or decrease infeas[idu]. Sensitivity:
  casadi_fill(d->w, p->nz, 0.);
  d->w[d->idu] = d->infeas[d->idu]>0 ? -1. : 1.;
  casadi_mv(d->nz_a, p->sp_a, d->w, d->w+p->nx, 0);
  // Find the best lam[i] to make zero
  best_ind = -1;
  best_w = 0.;
  for (i=0; i<p->nz; ++i) {
    // Make sure variable influences du
    if (d->w[i]==0.) continue;
    // Make sure removing the constraint decreases dual infeasibility
    if (d->w[i]>0. ? d->lam[i]>=0. : d->lam[i]<=0.) continue;
    // Skip if maximum infeasibility increases
    if (casadi_qp_du_check(d, i)>d->du) continue;
    // Check if best so far
    if (fabs(d->w[i])>best_w) {
      best_w = fabs(d->w[i]);
      best_ind = i;
    }
  }
  // Accept, if any
  if (best_ind>=0) {
    *sign = 0;
    casadi_qp_log(d, "Removed %lld to reduce |du|", best_ind);
    return best_ind;
  } else {
    return -1;
  }
}

// SYMBOL "qp_kkt"
template<typename T1>
void casadi_qp_kkt(casadi_qp_data<T1>* d) {
  // Local variables
  casadi_int i, k;
  const casadi_int *h_colind, *h_row, *a_colind, *a_row, *at_colind, *at_row,
                   *kkt_colind, *kkt_row;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  a_row = (a_colind = p->sp_a+2) + p->nx + 1;
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  h_row = (h_colind = p->sp_h+2) + p->nx + 1;
  kkt_row = (kkt_colind = p->sp_kkt+2) + p->nz + 1;
  // Reset w to zero
  casadi_fill(d->w, p->nz, 0.);
  // Loop over rows of the (transposed) KKT
  for (i=0; i<p->nz; ++i) {
    // Copy row of KKT to w
    if (i<p->nx) {
      if (d->lam[i]==0) {
        for (k=h_colind[i]; k<h_colind[i+1]; ++k) d->w[h_row[k]] = d->nz_h[k];
        for (k=a_colind[i]; k<a_colind[i+1]; ++k) d->w[p->nx+a_row[k]] = d->nz_a[k];
      } else {
        d->w[i] = 1.;
      }
    } else {
      if (d->lam[i]==0) {
        d->w[i] = -1.;
      } else {
        for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
          d->w[at_row[k]] = d->nz_at[k];
        }
      }
    }
    // Copy row to KKT, zero out w
    for (k=kkt_colind[i]; k<kkt_colind[i+1]; ++k) {
      d->nz_kkt[k] = d->w[kkt_row[k]];
      d->w[kkt_row[k]] = 0;
    }
  }
}

// SYMBOL "qp_kkt_vector"
template<typename T1>
void casadi_qp_kkt_vector(casadi_qp_data<T1>* d, T1* kkt_i, casadi_int i) {
  // Local variables
  casadi_int k;
  const casadi_int *h_colind, *h_row, *a_colind, *a_row, *at_colind, *at_row;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  a_row = (a_colind = p->sp_a+2) + p->nx + 1;
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  h_row = (h_colind = p->sp_h+2) + p->nx + 1;
  // Reset kkt_i to zero
  casadi_fill(kkt_i, p->nz, 0.);
  // Copy sparse entries
  if (i<p->nx) {
    for (k=h_colind[i]; k<h_colind[i+1]; ++k) kkt_i[h_row[k]] = d->nz_h[k];
    for (k=a_colind[i]; k<a_colind[i+1]; ++k) kkt_i[p->nx+a_row[k]] = d->nz_a[k];
  } else {
    for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
      kkt_i[at_row[k]] = -d->nz_at[k];
    }
  }
  // Add diagonal entry
  kkt_i[i] -= 1.;
}

// SYMBOL "qp_kkt_column"
template<typename T1>
void casadi_qp_kkt_column(casadi_qp_data<T1>* d, T1* kkt_i, casadi_int i,
                          casadi_int sign) {
  // Local variables
  casadi_int k;
  const casadi_int *h_colind, *h_row, *a_colind, *a_row, *at_colind, *at_row;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  a_row = (a_colind = p->sp_a+2) + p->nx + 1;
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  h_row = (h_colind = p->sp_h+2) + p->nx + 1;
  // Reset kkt_i to zero
  casadi_fill(kkt_i, p->nz, 0.);
  // Copy column of KKT to kkt_i
  if (i<p->nx) {
    if (sign==0) {
      for (k=h_colind[i]; k<h_colind[i+1]; ++k) kkt_i[h_row[k]] = d->nz_h[k];
      for (k=a_colind[i]; k<a_colind[i+1]; ++k) kkt_i[p->nx+a_row[k]] = d->nz_a[k];
    } else {
      kkt_i[i] = 1.;
    }
  } else {
    if (sign==0) {
      kkt_i[i] = -1.;
    } else {
      for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
        kkt_i[at_row[k]] = d->nz_at[k];
      }
    }
  }
}

// SYMBOL "qp_kkt_dot"
template<typename T1>
T1 casadi_qp_kkt_dot(casadi_qp_data<T1>* d, const T1* v, casadi_int i, casadi_int sign) {
  // Local variables
  casadi_int k;
  const casadi_int *h_colind, *h_row, *a_colind, *a_row, *at_colind, *at_row;
  T1 r;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  a_row = (a_colind = p->sp_a+2) + p->nx + 1;
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  h_row = (h_colind = p->sp_h+2) + p->nx + 1;
  // Scalar product with the desired column
  if (i<p->nx) {
    if (sign==0) {
      r = 0.;
      for (k=h_colind[i]; k<h_colind[i+1]; ++k) r += v[h_row[k]] * d->nz_h[k];
      for (k=a_colind[i]; k<a_colind[i+1]; ++k) r += v[p->nx+a_row[k]] * d->nz_a[k];
    } else {
      r = v[i];
    }
  } else {
    if (sign==0) {
      r = -v[i];
    } else {
      r = 0.;
      for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
        r += v[at_row[k]] * d->nz_at[k];
      }
    }
  }
  return r;
}

// SYMBOL "qp_kkt_dot2"
template<typename T1>
T1 casadi_qp_kkt_dot2(casadi_qp_data<T1>* d, const T1* v, casadi_int i) {
  // Local variables
  casadi_int k;
  const casadi_int *h_colind, *h_row, *a_colind, *a_row, *at_colind, *at_row;
  T1 r;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  a_row = (a_colind = p->sp_a+2) + p->nx + 1;
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  h_row = (h_colind = p->sp_h+2) + p->nx + 1;
  // Scalar product with the diagonal
  r = v[i];
  // Scalar product with the sparse entries
  if (i<p->nx) {
    for (k=h_colind[i]; k<h_colind[i+1]; ++k) r -= v[h_row[k]] * d->nz_h[k];
    for (k=a_colind[i]; k<a_colind[i+1]; ++k) r -= v[p->nx+a_row[k]] * d->nz_a[k];
  } else {
    for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
      r += v[at_row[k]] * d->nz_at[k];
    }
  }
  return r;
}

// SYMBOL "qp_kkt_residual"
template<typename T1>
void casadi_qp_kkt_residual(casadi_qp_data<T1>* d, T1* r) {
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  for (i=0; i<p->nz; ++i) {
    if (d->lam[i]>0.) {
      r[i] = d->ubz[i]-d->z[i];
    } else if (d->lam[i]<0.) {
      r[i] = d->lbz[i]-d->z[i];
    } else if (i<p->nx) {
      r[i] = d->lam[i]-d->infeas[i];
    } else {
      r[i] = d->lam[i];
    }
  }
}

// SYMBOL "qp_zero_blocking"
template<typename T1>
int casadi_qp_zero_blocking(casadi_qp_data<T1>* d, T1 e,
                            casadi_int* index, casadi_int* sign) {
  // Local variables
  T1 dz_max;
  casadi_int i;
  int ret;
  const casadi_qp_prob<T1>* p = d->prob;
  ret = 0;
  dz_max = 0.;
  for (i=0; i<p->nz; ++i) {
    if (-d->dz[i]>dz_max && d->z[i]<=d->lbz[i]-e) {
      ret = 1;
      if (index) *index = i;
      if (sign) *sign = -1;
      casadi_qp_log(d, "lbz[%lld] violated at 0", i);
    } else if (d->dz[i]>dz_max && d->z[i]>=d->ubz[i]+e) {
      ret = 1;
      if (index) *index = i;
      if (sign) *sign = 1;
      casadi_qp_log(d, "ubz[%lld] violated at 0", i);
    }
  }
  return ret;
}

// SYMBOL "qp_primal_blocking"
template<typename T1>
void casadi_qp_primal_blocking(casadi_qp_data<T1>* d, T1 e,
                               casadi_int* index, casadi_int* sign) {
  // Local variables
  casadi_int i;
  T1 trial_z;
  const casadi_qp_prob<T1>* p = d->prob;
  // Check if violation with tau=0 and not improving
  if (casadi_qp_zero_blocking(d, e, index, sign)) {
    d->tau = 0.;
    return;
  }
  // Loop over all primal variables
  for (i=0; i<p->nz; ++i) {
    if (d->dz[i]==0.) continue; // Skip zero steps
    // Trial primal step
    trial_z=d->z[i] + d->tau*d->dz[i];
    if (d->dz[i]<0 && trial_z<d->lbz[i]-e) {
      // Trial would increase maximum infeasibility
      d->tau = (d->lbz[i]-e-d->z[i])/d->dz[i];
      if (index) *index = d->lam[i]<0. ? -1 : i;
      if (sign) *sign = -1;
      casadi_qp_log(d, "Enforcing lbz[%lld]", i);
    } else if (d->dz[i]>0 && trial_z>d->ubz[i]+e) {
      // Trial would increase maximum infeasibility
      d->tau = (d->ubz[i]+e-d->z[i])/d->dz[i];
      if (index) *index = d->lam[i]>0. ? -1 : i;
      if (sign) *sign = 1;
      casadi_qp_log(d, "Enforcing ubz[%lld]", i);
    }
    if (d->tau<=0) return;
  }
}

// SYMBOL "qp_dual_breakpoints"
template<typename T1>
casadi_int casadi_qp_dual_breakpoints(casadi_qp_data<T1>* d, T1* tau_list,
                                      casadi_int* ind_list, T1 e, T1 tau) {
  // Local variables
  casadi_int i, n_tau, loc, next_ind, tmp_ind, j;
  T1 trial_lam, new_tau, next_tau, tmp_tau;
  const casadi_qp_prob<T1>* p = d->prob;
  // Dual feasibility is piecewise linear. Start with one interval [0,tau]:
  tau_list[0] = tau;
  ind_list[0] = -1; // no associated index
  n_tau = 1;
  // Find the taus corresponding to lam crossing zero and insert into list
  for (i=0; i<p->nz; ++i) {
    if (d->dlam[i]==0.) continue; // Skip zero steps
    if (d->lam[i]==0.) continue; // Skip inactive constraints
    // Trial dual step
    trial_lam = d->lam[i] + tau*d->dlam[i];
    // Skip if no sign change
    if (d->lam[i]>0 ? trial_lam>=0 : trial_lam<=0) continue;
    // Location of the sign change
    new_tau = -d->lam[i]/d->dlam[i];
    // Where to insert the w[i]
    for (loc=0; loc<n_tau-1; ++loc) {
      if (new_tau<tau_list[loc]) break;
    }
    // Insert element
    n_tau++;
    next_tau=new_tau;
    next_ind=i;
    for (j=loc; j<n_tau; ++j) {
      tmp_tau = tau_list[j];
      tau_list[j] = next_tau;
      next_tau = tmp_tau;
      tmp_ind = ind_list[j];
      ind_list[j] = next_ind;
      next_ind = tmp_ind;
    }
  }
  return n_tau;
}

// SYMBOL "qp_dual_blocking"
template<typename T1>
casadi_int casadi_qp_dual_blocking(casadi_qp_data<T1>* d, T1 e) {
  // Local variables
  casadi_int i, n_tau, j, k, du_index;
  T1 tau_k, dtau, new_infeas, tau1;
  const casadi_int *at_colind, *at_row;
  const casadi_qp_prob<T1>* p = d->prob;
  // Extract sparsities
  at_row = (at_colind = p->sp_at+2) + p->na + 1;
  // Dual feasibility is piecewise linear in tau. Get the intervals:
  n_tau = casadi_qp_dual_breakpoints(d, d->w, d->iw, e, d->tau);
  // No dual blocking yet
  du_index = -1;
  // How long step can we take without exceeding e?
  tau_k = 0.;
  for (j=0; j<n_tau; ++j) {
    // Distance to the next tau (may be zero)
    dtau = d->w[j] - tau_k;
    // Check if maximum dual infeasibilty gets exceeded
    for (k=0; k<p->nx; ++k) {
      new_infeas = d->infeas[k]+dtau*d->tinfeas[k];
      if (fabs(new_infeas)>e) {
        tau1 = fmax(0., tau_k + ((new_infeas>0 ? e : -e)-d->infeas[k])/d->tinfeas[k]);
        if (tau1 < d->tau) {
          // Smallest tau found so far
          d->tau = tau1;
          du_index = k;
        }
      }
    }
    // Update infeasibility
    casadi_axpy(p->nx, fmin(d->tau - tau_k, dtau), d->tinfeas, d->infeas);
    // Stop here if dual blocking constraint
    if (du_index>=0) return du_index;
    // Continue to the next tau
    tau_k = d->w[j];
    // Get component, break if last
    i = d->iw[j];
    if (i<0) break;
    // Update sign or tinfeas
    if (!d->neverzero[i]) {
      // lam becomes zero, update the infeasibility tangent
      if (i<p->nx) {
        // Set a lam_x to zero
        d->tinfeas[i] -= d->dlam[i];
      } else {
        // Set a lam_a to zero
        for (k=at_colind[i-p->nx]; k<at_colind[i-p->nx+1]; ++k) {
          d->tinfeas[at_row[k]] -= d->nz_at[k]*d->dlam[i];
        }
      }
    }
  }
  return du_index;
}

// SYMBOL "qp_take_step"
template<typename T1>
void casadi_qp_take_step(casadi_qp_data<T1>* d) {
  // Local variables
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  // Get current sign
  for (i=0; i<p->nz; ++i) d->iw[i] = d->lam[i]>0. ? 1 : d->lam[i]<0 ? -1 : 0;
  // Take primal-dual step
  casadi_axpy(p->nz, d->tau, d->dz, d->z);
  casadi_axpy(p->nz, d->tau, d->dlam, d->lam);
  // Update sign
  for (i=0; i<p->nz; ++i) {
    // Allow sign changes for certain components
    if (d->neverzero[i] && (d->iw[i]<0 ? d->lam[i]>0 : d->lam[i]<0)) {
      d->iw[i]=-d->iw[i];
    }
    // Ensure correct sign
    switch (d->iw[i]) {
      case -1: d->lam[i] = fmin(d->lam[i], -p->dmin); break;
      case  1: d->lam[i] = fmax(d->lam[i],  p->dmin); break;
      case  0: d->lam[i] = 0.; break;
    }
  }
}

// SYMBOL "qp_flip_check"
template<typename T1>
int casadi_qp_flip_check(casadi_qp_data<T1>* d, casadi_int index, casadi_int sign,
                         casadi_int* r_index, T1* r_lam, T1 e) {
  // Local variables
  casadi_int i;
  T1 best_duerr, new_duerr, r, new_lam;
  const casadi_qp_prob<T1>* p = d->prob;
  // Try to find a linear combination of the new columns
  casadi_qp_kkt_vector(d, d->dz, index);
  casadi_qr_solve(d->dz, 1, 0, p->sp_v, d->nz_v, p->sp_r, d->nz_r, d->beta,
                  p->prinv, p->pc, d->w);
  // If |dz|==0, columns are linearly independent
  r = sqrt(casadi_dot(p->nz, d->dlam, d->dlam));
  if (r<1e-12) return 0;
  // Normalize dz
  casadi_scal(p->nz, 1./r, d->dz);
  // Find a linear combination of the new rows
  casadi_qp_kkt_vector(d, d->dlam, index);
  casadi_qr_solve(d->dlam, 1, 1, p->sp_v, d->nz_v, p->sp_r, d->nz_r, d->beta,
                  p->prinv, p->pc, d->w);
  // If |dlam|==0, rows are linearly independent (due to bad scaling?)
  r = sqrt(casadi_dot(p->nz, d->dlam, d->dlam));
  if (r<1e-12) return 0;
  // Normalize dlam
  casadi_scal(p->nz, 1./r, d->dlam);


  // New column that we're trying to add
  casadi_qp_kkt_column(d, d->dz, index, sign);
  // Express it using the other columns
  casadi_qr_solve(d->dz, 1, 0, p->sp_v, d->nz_v, p->sp_r, d->nz_r, d->beta,
                  p->prinv, p->pc, d->w);
  // Quick return if columns are linearly independent
  if (fabs(d->dz[index])>=1e-12) {
//    printf("old rule: |dz| = %g\n", r);

    return 0;
  }
  // Column that we're removing
  casadi_qp_kkt_column(d, d->w, index, !sign);
  casadi_scal(p->nz, 1./sqrt(casadi_dot(p->nz, d->w, d->w)), d->w);
  // Find best constraint we can flip, if any
  *r_index=-1;
  *r_lam=0;
  best_duerr = inf;
  for (i=0; i<p->nz; ++i) {
    // Can't be the same
    if (i==index) continue;
    // Make sure constraint is flippable
    if (d->lam[i]==0 ? d->neverlower[i] && d->neverupper[i] : d->neverzero[i]) continue;
    // If dz[i]==0, column i is redundant
    if (fabs(d->dz[i])<1e-12) continue;
    // If dot(dlam, kkt_diff(i))==0, rank won't increase
    if (fabs(casadi_qp_kkt_dot2(d, d->dlam, i))<1e-12) continue;
    // Get new sign
    if (d->lam[i]==0.) {
      new_lam = d->lbz[i]-d->z[i] >= d->z[i]-d->ubz[i] ? -p->dmin : p->dmin;
      continue;
    } else {
      new_lam = 0;
    }
    // New dual error (for affected subset)
    new_duerr = casadi_qp_du_check(d, i);


    T1 viol = d->z[i]>d->ubz[i] ? d->z[i]-d->ubz[i] : d->z[i]<d->lbz[i] ? d->lbz[i]-d->z[i] : 0;

    printf("candidate %lld: lam=%g, z=%g, ubz=%g, lbz=%g, du_check=%g, e=%g, dot=%g, viol=%g\n",
           i, d->lam[i], d->z[i], d->ubz[i], d->lbz[i],
           new_duerr, e, casadi_qp_kkt_dot(d, d->w, i, d->lam[i]==0.), viol);

    if (i!=12 && fabs(casadi_qp_kkt_dot(d, d->w, i, d->lam[i]==0.)) < 1e-12) continue;

    // Best so far?
    if (new_duerr < best_duerr) {
      best_duerr = new_duerr;
      *r_index = i;
      *r_lam = new_lam;
    }
  }
  // Enforcing will lead to (hopefully recoverable) singularity
  return 1;
}

// SYMBOL "qp_factorize"
template<typename T1>
void casadi_qp_factorize(casadi_qp_data<T1>* d) {
  const casadi_qp_prob<T1>* p = d->prob;
  // Construct the KKT matrix
  casadi_qp_kkt(d);
  // QR factorization
  casadi_qr(p->sp_kkt, d->nz_kkt, d->w, p->sp_v, d->nz_v, p->sp_r,
            d->nz_r, d->beta, p->prinv, p->pc);
  // Check singularity
  d->sing = casadi_qr_singular(&d->mina, &d->imina, d->nz_r, p->sp_r, p->pc, 1e-12);
}

// SYMBOL "qp_scale_step"
template<typename T1>
int casadi_qp_scale_step(casadi_qp_data<T1>* d, casadi_int* r_index, casadi_int* r_sign) {
  // Local variables
  T1 tpr, tdu, terr, tau_test, minat_tr, tau;
  int pos_ok, neg_ok;
  casadi_int nnz_kkt, nullity_tr, nulli, imina_tr, i;
  const casadi_qp_prob<T1>* p = d->prob;
  // Reset r_index, r_sign, quick return if non-singular
  *r_index = -1;
  *r_sign = 0;
  if (!d->sing) {
    return 0;
  }
  // Change in pr, du in the search direction
  tpr = d->ipr<0 ? 0.
                 : d->z[d->ipr]>d->ubz[d->ipr] ? d->dz[d->ipr]/d->pr
                                               : -d->dz[d->ipr]/d->pr;
  tdu = d->idu<0 ? 0. : d->tinfeas[d->idu]/d->infeas[d->idu];
  // Change in max(pr, du) in the search direction
  pos_ok=1, neg_ok=1;
  if (d->pr>d->du) {
    // |pr|>|du|
    if (tpr<0) {
      neg_ok = 0;
    } else if (tpr>0) {
      pos_ok = 0;
    }
    terr = tpr;
  } else if (d->pr<d->du) {
    // |pr|<|du|
    if (tdu<0) {
      neg_ok = 0;
    } else if (tdu>0) {
      pos_ok = 0;
    }
    terr = tdu;
  } else {
    // |pr|==|du|
    if ((tpr>0 && tdu<0) || (tpr<0 && tdu>0)) {
      // |pr|==|du| cannot be decreased along the search direction
      pos_ok = neg_ok = 0;
      terr = 0;
    } else if (fmin(tpr, tdu)<0) {
      // |pr|==|du| decreases for positive tau
      neg_ok = 0;
      terr = fmax(tpr, tdu);
    } else if (fmax(tpr, tdu)>0) {
      // |pr|==|du| decreases for negative tau
      pos_ok = 0;
      terr = fmin(tpr, tdu);
    } else {
      terr = 0;
    }
  }
  // If primal error is dominating and constraint is active,
  // then only allow the multiplier to become larger
  if (p->du_to_pr*d->pr>=d->du && d->lam[d->ipr]!=0 && fabs(d->dlam[d->ipr])>1e-12) {
    if ((d->lam[d->ipr]>0)==(d->dlam[d->ipr]>0)) {
      neg_ok = 0;
    } else {
      pos_ok = 0;
    }
  }
  // QR factorization of the transpose
  casadi_trans(d->nz_kkt, p->sp_kkt, d->nz_v, p->sp_kkt, d->iw);
  nnz_kkt = p->sp_kkt[2+p->nz]; // kkt_colind[nz]
  casadi_copy(d->nz_v, nnz_kkt, d->nz_kkt);
  casadi_qr(p->sp_kkt, d->nz_kkt, d->w, p->sp_v, d->nz_v, p->sp_r, d->nz_r,
            d->beta, p->prinv, p->pc);
  // Best flip
  tau = p->inf;
  // For all nullspace vectors
  nullity_tr = casadi_qr_singular(&minat_tr, &imina_tr, d->nz_r, p->sp_r,
                                  p->pc, 1e-12);
  for (nulli=0; nulli<nullity_tr; ++nulli) {
    // Get a linear combination of the rows in kkt
    casadi_qr_colcomb(d->w, d->nz_r, p->sp_r, p->pc, imina_tr, nulli);
    // Look for the best constraint for increasing rank
    for (i=0; i<p->nz; ++i) {
      // Check if old column can be removed without decreasing rank
      if (fabs(i<p->nx ? d->dz[i] : d->dlam[i])<1e-12) continue;
      // If dot(w, kkt_diff(i))==0, rank won't increase
      if (fabs(casadi_qp_kkt_dot2(d, d->w, i))<1e-12) continue;
      // Is constraint active?
      if (d->lam[i]==0.) {
        // Make sure that step is nonzero
        if (fabs(d->dz[i])<1e-12) continue;
        // Step needed to bring z to lower bound
        if (!d->neverlower[i]) {
          tau_test = (d->lbz[i]-d->z[i])/d->dz[i];
          // Ensure nonincrease in max(pr, du)
          if (!((terr>0. && tau_test>0.) || (terr<0. && tau_test<0.))) {
            // Only allow removing constraints if tau_test==0
            if (fabs(tau_test)>=1e-16) {
              // Check if best so far
              if (fabs(tau_test)<fabs(tau)) {
                tau = tau_test;
                *r_index = i;
                *r_sign = -1;
                casadi_qp_log(d, "Enforced lbz[%lld] for regularity", i);
              }
            }
          }
        }
        // Step needed to bring z to upper bound
        if (!d->neverupper[i]) {
          tau_test = (d->ubz[i]-d->z[i])/d->dz[i];
          // Ensure nonincrease in max(pr, du)
          if (!((terr>0. && tau_test>0.) || (terr<0. && tau_test<0.))) {
            // Only allow removing constraints if tau_test==0
            if (fabs(tau_test)>=1e-16) {
              // Check if best so far
              if (fabs(tau_test)<fabs(tau)) {
                tau = tau_test;
                *r_index = i;
                *r_sign = 1;
                casadi_qp_log(d, "Enforced ubz[%lld] for regularity", i);
              }
            }
          }
        }
      } else {
        // Make sure that step is nonzero
        if (fabs(d->dlam[i])<1e-12) continue;
        // Step needed to bring lam to zero
        if (!d->neverzero[i]) {
          tau_test = -d->lam[i]/d->dlam[i];
          // Ensure nonincrease in max(pr, du)
          if ((terr>0. && tau_test>0.) || (terr<0. && tau_test<0.)) continue;
          // Make sure direction is permitted
          if ((tau_test>0 && !pos_ok) || (tau_test<0 && !neg_ok)) continue;
          // Check if best so far
          if (fabs(tau_test)<fabs(tau)) {
            tau = tau_test;
            *r_index = i;
            *r_sign = 0;
            casadi_qp_log(d, "Dropped %s[%lld] for regularity",
                   d->lam[i]>0 ? "lbz" : "ubz", i);
          }
        }
      }
    }
  }
  // Can we restore feasibility?
  if (*r_index<0) return 1;
  // Scale step so that that tau=1 corresponds to a full step
  casadi_scal(p->nz, tau, d->dz);
  casadi_scal(p->nz, tau, d->dlam);
  casadi_scal(p->nx, tau, d->tinfeas);
  return 0;
}

// SYMBOL "qp_calc_step"
template<typename T1>
int casadi_qp_calc_step(casadi_qp_data<T1>* d, casadi_int* r_index, casadi_int* r_sign) {
  // Local variables
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  // Calculate step in z[:nx] and lam[nx:]
  if (!d->sing) {
    // Negative KKT residual
    casadi_qp_kkt_residual(d, d->dz);
    // Solve to get primal-dual step
    casadi_qr_solve(d->dz, 1, 1, p->sp_v, d->nz_v, p->sp_r, d->nz_r, d->beta,
                    p->prinv, p->pc, d->w);
  } else {
    // Get a linear combination of the columns in KKT
    casadi_qr_colcomb(d->dz, d->nz_r, p->sp_r, p->pc, d->imina, 0);
  }
  // Calculate change in Lagrangian gradient
  casadi_fill(d->dlam, p->nx, 0.);
  casadi_mv(d->nz_h, p->sp_h, d->dz, d->dlam, 0); // gradient of the objective
  casadi_mv(d->nz_a, p->sp_a, d->dz+p->nx, d->dlam, 1); // gradient of the Lagrangian
  // Step in lam[:nx]
  casadi_scal(p->nx, -1., d->dlam);
  // For inactive constraints, lam(x) step is zero
  for (i=0; i<p->nx; ++i) if (d->lam[i]==0.) d->dlam[i] = 0.;
  // Step in lam[nx:]
  casadi_copy(d->dz+p->nx, p->na, d->dlam+p->nx);
  // Step in z[nx:]
  casadi_fill(d->dz+p->nx, p->na, 0.);
  casadi_mv(d->nz_a, p->sp_a, d->dz, d->dz+p->nx, 0);
  // Avoid steps that are nonzero due to numerics
  for (i=0; i<p->nz; ++i) if (fabs(d->dz[i])<1e-14) d->dz[i] = 0.;
  // Tangent of the dual infeasibility at tau=0
  casadi_fill(d->tinfeas, p->nx, 0.);
  casadi_mv(d->nz_h, p->sp_h, d->dz, d->tinfeas, 0);
  casadi_mv(d->nz_a, p->sp_a, d->dlam+p->nx, d->tinfeas, 1);
  casadi_axpy(p->nx, 1., d->dlam, d->tinfeas);
  // Calculate step length
  return casadi_qp_scale_step(d, r_index, r_sign);
}

// SYMBOL "qp_calc_dependent"
template<typename T1>
void casadi_qp_calc_dependent(casadi_qp_data<T1>* d) {
  // Local variables
  casadi_int i;
  const casadi_qp_prob<T1>* p = d->prob;
  // Calculate f
  d->f = casadi_bilin(d->nz_h, p->sp_h, d->z, d->z)/2.
       + casadi_dot(p->nx, d->z, d->g);
  // Calculate z[nx:]
  casadi_fill(d->z+p->nx, p->na, 0.);
  casadi_mv(d->nz_a, p->sp_a, d->z, d->z+p->nx, 0);
  // Calculate gradient of the Lagrangian
  casadi_copy(d->g, p->nx, d->infeas);
  casadi_mv(d->nz_h, p->sp_h, d->z, d->infeas, 0);
  casadi_mv(d->nz_a, p->sp_a, d->lam+p->nx, d->infeas, 1);
  // Calculate lam[:nx] without changing the sign, dual infeasibility
  for (i=0; i<p->nx; ++i) {
    if (d->lam[i]>0) {
      d->lam[i] = fmax(-d->infeas[i], p->dmin);
    } else if (d->lam[i]<0) {
      d->lam[i] = fmin(-d->infeas[i], -p->dmin);
    }
    d->infeas[i] += d->lam[i];
  }
  // Calculate primal and dual error
  casadi_qp_pr(d);
  casadi_qp_du(d);
}

template<typename T1>
void casadi_qp_linesearch(casadi_qp_data<T1>* d, casadi_int* index, casadi_int* sign) {
  const casadi_qp_prob<T1>* p = d->prob;
  // Start with a full step and no active set change
  *sign=0;
  *index=-1;
  d->tau = 1.;
  // Find largest possible step without exceeding acceptable |pr|
  casadi_qp_primal_blocking(d, fmax(d->pr, d->du/p->du_to_pr), index, sign);
  // Find largest possible step without exceeding acceptable |du|
  if (casadi_qp_dual_blocking(d, fmax(d->pr*p->du_to_pr, d->du))>=0) {
    *index = -1;
    *sign=0;
  }
  // Take primal-dual step, avoiding accidental sign changes for lam
  casadi_qp_take_step(d);
}

template<typename T1>
void casadi_qp_flip(casadi_qp_data<T1>* d, casadi_int *index, casadi_int *sign,
                                          casadi_int r_index, casadi_int r_sign) {
  // Local variables
  T1 e, r_lam;
  const casadi_qp_prob<T1>* p = d->prob;
  // acceptable dual error
  e = fmax(p->du_to_pr*d->pr, d->du);
  // Try to restore regularity if possible
  if (r_index>=0 && (r_sign!=0 || casadi_qp_du_check(d, r_index)<=e)) {
    *index = r_index;
    *sign = r_sign;
    casadi_qp_log(d, "%lld->%lld for regularity", *index, *sign);
  }
  // Improve primal or dual feasibility
  if (*index==-1 && d->tau>1e-16 && (d->ipr>=0 || d->idu>=0)) {
    if (p->du_to_pr*d->pr >= d->du) {
      // Try to improve primal feasibility
      *index = casadi_qp_pr_index(d, sign);
    } else {
      // Try to improve dual feasibility
      *index = casadi_qp_du_index(d, sign);
    }
  }
  // If a constraint was added
  if (*index>=0) {
    // Try to maintain non-singularity if possible
    if (!d->sing && casadi_qp_flip_check(d, *index, *sign, &r_index, &r_lam, e)) {
      if (r_index>=0) {
        // Also flip r_index to avoid singularity
        d->lam[r_index] = r_lam;
        casadi_qp_log(d, "%lld->%lld, %lld->%g", *index, *sign, r_index, r_lam);
      } else if (*sign!=0) {
        // Do not allow singularity created from adding a constraint, abort
        casadi_qp_log(d, "Cannot enforce %s[%lld]", *sign>0 ? "ubz" : "lbz", *index);
        *index = -1;
        return;
      }
    }
    d->lam[*index] = *sign==0 ? 0 : *sign>0 ? p->dmin : -p->dmin;
    // Recalculate primal and dual infeasibility
    casadi_qp_calc_dependent(d);
    // Reset index
    *index=-2;
  }
}