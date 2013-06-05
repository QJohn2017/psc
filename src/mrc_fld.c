
#include <mrc_fld.h>

#include <mrc_vec.h>
#include <mrc_io.h>
#include <mrc_params.h>
#include <mrc_profile.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// ======================================================================
// mrc_fld

typedef void (*mrc_fld_copy_to_func_t)(struct mrc_fld *,
				       struct mrc_fld *);
typedef void (*mrc_fld_copy_from_func_t)(struct mrc_fld *,
					 struct mrc_fld *);

// ----------------------------------------------------------------------
// mrc_fld_destroy

static void
_mrc_fld_destroy(struct mrc_fld *fld)
{
  mrc_vec_put_array(fld->_vec, fld->_arr);
  fld->_arr = NULL;

  for (int m = 0; m < fld->_nr_allocated_comp_name; m++) {
    free(fld->_comp_name[m]);
  }
  free(fld->_comp_name);
}

// ----------------------------------------------------------------------
// mrc_fld_setup

static void
_mrc_fld_setup(struct mrc_fld *fld)
{
  if (fld->_offs.nr_vals == 0) {
    mrc_fld_set_param_int_array(fld, "offs", fld->_dims.nr_vals, NULL);
  }
  if (fld->_sw.nr_vals == 0) {
    mrc_fld_set_param_int_array(fld, "sw", fld->_dims.nr_vals, NULL);
  }
  assert(fld->_dims.nr_vals == fld->_offs.nr_vals &&
	 fld->_dims.nr_vals == fld->_sw.nr_vals);
  assert(fld->_dims.nr_vals <= MRC_FLD_MAXDIMS);

  fld->_len = 1;
  for (int d = 0; d < fld->_dims.nr_vals; d++) {
    fld->_ghost_offs[d] = fld->_offs.vals[d] - fld->_sw.vals[d];
    fld->_ghost_dims[d] = fld->_dims.vals[d] + 2 * fld->_sw.vals[d];
    fld->_len *= fld->_ghost_dims[d];
  }

  mrc_vec_set_type(fld->_vec, mrc_fld_type(fld));
  mrc_vec_set_param_int(fld->_vec, "len", fld->_len);
  mrc_fld_setup_member_objs(fld); // sets up our .vec member

  fld->_arr = mrc_vec_get_array(fld->_vec);
}

// ----------------------------------------------------------------------
// mrc_fld_set_array

void
mrc_fld_set_array(struct mrc_fld *fld, void *arr)
{
  mrc_vec_set_array(fld->_vec, arr);
}

// ----------------------------------------------------------------------
// mrc_fld_write

static void
_mrc_fld_write(struct mrc_fld *fld, struct mrc_io *io)
{
  mrc_io_write_ref(io, fld, "domain", fld->_domain);
  mrc_io_write_fld(io, mrc_io_obj_path(io, fld), fld);
}

// ----------------------------------------------------------------------
// mrc_fld_read

static void
_mrc_fld_read(struct mrc_fld *fld, struct mrc_io *io)
{
  fld->_domain = mrc_io_read_ref(io, fld, "domain", mrc_domain);

  // instead of reading back fld->_vec (which doesn't contain anything useful,
  // anyway, since mrc_fld saves/restores the data rather than mrc_vec),
  // we make a new one, so at least we're sure that with_array won't be honored
  fld->_vec = mrc_vec_create(mrc_fld_comm(fld));
  mrc_fld_setup(fld);
  mrc_io_read_fld(io, mrc_io_obj_path(io, fld), fld);
}

// ----------------------------------------------------------------------
// mrc_fld_set_comp_name

void
mrc_fld_set_comp_name(struct mrc_fld *fld, int m, const char *name)
{
  int nr_comps = mrc_fld_nr_comps(fld);
  assert(m < nr_comps);
  if (nr_comps > fld->_nr_allocated_comp_name) {
    for (int i = 0; i < fld->_nr_allocated_comp_name; i++) {
      free(fld->_comp_name[m]);
    }
    free(fld->_comp_name);
    fld->_comp_name = calloc(nr_comps, sizeof(*fld->_comp_name));
    fld->_nr_allocated_comp_name = nr_comps;
  }
  free(fld->_comp_name[m]);
  fld->_comp_name[m] = name ? strdup(name) : NULL;
}

// ----------------------------------------------------------------------
// mrc_fld_comp_name

const char *
mrc_fld_comp_name(struct mrc_fld *fld, int m)
{
  assert(m < mrc_fld_nr_comps(fld) && m < fld->_nr_allocated_comp_name);
  return fld->_comp_name[m];
}

// ----------------------------------------------------------------------
// mrc_fld_nr_comps

int
mrc_fld_nr_comps(struct mrc_fld *fld)
{
  int comp_dim = fld->_dims.nr_vals - 1;
  assert(comp_dim >= 0);
  return fld->_dims.vals[comp_dim];
}

// ----------------------------------------------------------------------
// mrc_fld_set_nr_comps

void
mrc_fld_set_nr_comps(struct mrc_fld *fld, int nr_comps)
{
  int comp_dim = fld->_dims.nr_vals - 1;
  fld->_dims.vals[comp_dim] = nr_comps;
}

// ----------------------------------------------------------------------
// mrc_fld_offs

const int *
mrc_fld_offs(struct mrc_fld *fld)
{
  return fld->_offs.vals;
}

// ----------------------------------------------------------------------
// mrc_fld_dims

const int *
mrc_fld_dims(struct mrc_fld *fld)
{
  return fld->_dims.vals;
}

// ----------------------------------------------------------------------
// mrc_fld_ghost_offs

const int *
mrc_fld_ghost_offs(struct mrc_fld *fld)
{
  return fld->_ghost_offs;
}

// ----------------------------------------------------------------------
// mrc_fld_ghost_dims

const int *
mrc_fld_ghost_dims(struct mrc_fld *fld)
{
  return fld->_ghost_dims;
}

// ----------------------------------------------------------------------
// mrc_fld_duplicate

struct mrc_fld *
mrc_fld_duplicate(struct mrc_fld *fld)
{
  struct mrc_fld *fld_new = mrc_fld_create(mrc_fld_comm(fld));
  mrc_fld_set_param_int_array(fld_new, "dims", fld->_dims.nr_vals, fld->_dims.vals);
  mrc_fld_set_param_int_array(fld_new, "offs", fld->_offs.nr_vals, fld->_offs.vals);
  mrc_fld_set_param_int_array(fld_new, "sw", fld->_sw.nr_vals, fld->_sw.vals);
  fld_new->_domain = fld->_domain;
  mrc_fld_setup(fld_new);
  return fld_new;
}

// ----------------------------------------------------------------------
// mrc_fld_copy

void
mrc_fld_copy(struct mrc_fld *fld_to, struct mrc_fld *fld_from)
{
  assert(mrc_fld_same_shape(fld_to, fld_from));

  memcpy(fld_to->_arr, fld_from->_arr, fld_to->_len * sizeof(float));
}

// ----------------------------------------------------------------------
// mrc_fld_axpy

void
mrc_fld_axpy(struct mrc_fld *y, float alpha, struct mrc_fld *x)
{
  assert(mrc_fld_same_shape(x, y));

  assert(y->_data_type == MRC_NT_FLOAT);
  float *y_arr = y->_arr, *x_arr = x->_arr;
  for (int i = 0; i < y->_len; i++) {
    y_arr[i] += alpha * x_arr[i];
  }
}

// ----------------------------------------------------------------------
// mrc_fld_waxpy

// FIXME, should take double alpha
void
mrc_fld_waxpy(struct mrc_fld *w, float alpha, struct mrc_fld *x, struct mrc_fld *y)
{
  assert(mrc_fld_same_shape(x, y));
  assert(mrc_fld_same_shape(x, w));

  assert(y->_data_type == MRC_NT_FLOAT);
  float *y_arr = y->_arr, *x_arr = x->_arr, *w_arr = w->_arr;
  for (int i = 0; i < y->_len; i++) {
    w_arr[i] = alpha * x_arr[i] + y_arr[i];
  }
}

// ----------------------------------------------------------------------
// mrc_fld_norm

// FIXME, should return double
float
mrc_fld_norm(struct mrc_fld *x)
{
  assert(x->_data_type == MRC_NT_FLOAT);
  assert(x->_dims.nr_vals == 4);
  int nr_comps = mrc_fld_nr_comps(x);
  float res = 0.;
  for (int m = 0; m < nr_comps; m++) {
    mrc_fld_foreach(x, ix, iy, iz, 0, 0) {
      res = fmaxf(res, fabsf(MRC_F3(x,m, ix,iy,iz)));
    } mrc_fld_foreach_end;
  }

  MPI_Allreduce(MPI_IN_PLACE, &res, 1, MPI_FLOAT, MPI_MAX, mrc_fld_comm(x));
  return res;
}

// ----------------------------------------------------------------------
// mrc_fld_set

void
mrc_fld_set(struct mrc_fld *x, float val)
{
  assert(x->_data_type == MRC_NT_FLOAT);
  float *arr = x->_arr;
  for (int i = 0; i < x->_len; i++) {
    arr[i] = val;
  }
}

// ----------------------------------------------------------------------
// mrc_fld_write_comps

void
mrc_fld_write_comps(struct mrc_fld *fld, struct mrc_io *io, int mm[])
{
  for (int i = 0; mm[i] >= 0; i++) {
    struct mrc_fld *fld1 = mrc_fld_create(mrc_fld_comm(fld));
    mrc_fld_set_param_int_array(fld1, "offs", fld->_offs.nr_vals, fld->_offs.vals);
    int *dims = fld->_dims.vals;
    mrc_fld_set_param_int_array(fld1, "dims", 4,
				(int[4]) { dims[0], dims[1], dims[2], 1 });
    mrc_fld_set_param_int_array(fld1, "sw", fld->_sw.nr_vals, fld->_sw.vals);
    int *ib = fld->_ghost_offs;
    mrc_fld_set_array(fld1, &MRC_F3(fld,mm[i], ib[0], ib[1], ib[2]));
    mrc_fld_set_name(fld1, fld->_comp_name[mm[i]]);
    mrc_fld_set_comp_name(fld1, 0, fld->_comp_name[mm[i]]);
    fld1->_domain = fld->_domain;
    mrc_fld_setup(fld1);
    mrc_fld_write(fld1, io);
    mrc_fld_destroy(fld1);
  }
}

// ----------------------------------------------------------------------
// mrc_fld_get_as
//
// convert fld_base to mrc_fld of type "type"

struct mrc_fld *
mrc_fld_get_as(struct mrc_fld *fld_base, const char *type)
{
  const char *type_base = mrc_fld_type(fld_base);
  // if we're already the subtype, nothing to be done
  if (strcmp(type_base, type) == 0)
    return fld_base;

  static int pr;
  if (!pr) {
    pr = prof_register("mrc_fld_get_as", 1., 0, 0);
  }
  prof_start(pr);

  struct mrc_fld *fld = mrc_fld_create(mrc_fld_comm(fld_base));
  mrc_fld_set_type(fld, type);
  mrc_fld_set_param_int_array(fld, "dims", fld_base->_dims.nr_vals, fld_base->_dims.vals);
  mrc_fld_set_param_int_array(fld, "offs", fld_base->_offs.nr_vals, fld_base->_offs.vals);
  mrc_fld_set_param_int_array(fld, "sw", fld_base->_sw.nr_vals, fld_base->_sw.vals);
  fld->_domain = fld_base->_domain;
  // FIXME, component names, too
  mrc_fld_setup(fld);

  char s[strlen(type) + 12]; sprintf(s, "copy_to_%s", type);
  mrc_fld_copy_to_func_t copy_to = (mrc_fld_copy_to_func_t)
    mrc_fld_get_method(fld_base, s);
  if (copy_to) {
    copy_to(fld_base, fld);
  } else {
    sprintf(s, "copy_from_%s", type_base);
    mrc_fld_copy_from_func_t copy_from = (mrc_fld_copy_from_func_t)
      mrc_fld_get_method(fld, s);
    if (copy_from) {
      copy_from(fld, fld_base);
    } else {
      fprintf(stderr, "ERROR: no 'copy_to_%s' in mrc_fld '%s' and "
	      "no 'copy_from_%s' in '%s'!\n",
	      type, mrc_fld_type(fld_base), type_base, mrc_fld_type(fld));
      assert(0);
    }
  }

  prof_stop(pr);
  return fld;
}

// ----------------------------------------------------------------------
// mrc_fld_put_as
//
// after being done with the fields gotten from get_as(), need to put them
// back using this routine, which will copy the contents from fld back
// to fld_base

void
mrc_fld_put_as(struct mrc_fld *fld, struct mrc_fld *fld_base)
{
  const char *type_base = mrc_fld_type(fld_base);
  const char *type = mrc_fld_type(fld);
  // If we're already the subtype, nothing to be done
  if (strcmp(type_base, type) == 0)
    return;

  static int pr;
  if (!pr) {
    pr = prof_register("mrc_fld_put_as", 1., 0, 0);
  }
  prof_start(pr);

  char s[strlen(type) + 12]; sprintf(s, "copy_from_%s", type);
  mrc_fld_copy_from_func_t copy_from = (mrc_fld_copy_from_func_t)
    mrc_fld_get_method(fld_base, s);
  if (copy_from) {
    copy_from(fld_base, fld);
  } else {
    sprintf(s, "copy_to_%s", type_base);
    mrc_fld_copy_to_func_t copy_to = (mrc_fld_copy_to_func_t)
      mrc_fld_get_method(fld, s);
    if (copy_to) {
      copy_to(fld, fld_base);
    } else {
      fprintf(stderr, "ERROR: no 'copy_from_%s' in mrc_fld '%s' and "
	      "no 'copy_to_%s' in '%s'!\n",
	      type, mrc_fld_type(fld_base), type_base, mrc_fld_type(fld));
      assert(0);
    }
  }

  mrc_fld_destroy(fld);

  prof_stop(pr);
}

// ======================================================================
// mrc_fld subclasses

// ----------------------------------------------------------------------
// mrc_fld_float_copy_from_double

static void
mrc_fld_float_copy_from_double(struct mrc_fld *fld_float,
			       struct mrc_fld *fld_double)
{
  assert(mrc_fld_same_shape(fld_float, fld_double));
  assert(fld_float->_data_type == MRC_NT_FLOAT);
  assert(fld_double->_data_type == MRC_NT_DOUBLE);
  float *f_arr = fld_float->_arr;
  double *d_arr = fld_double->_arr;
  for (int i = 0; i < fld_float->_len; i++) {
    f_arr[i] = d_arr[i];
  }
}

// ----------------------------------------------------------------------
// mrc_fld_float_copy_to_double

static void
mrc_fld_float_copy_to_double(struct mrc_fld *fld_float,
			     struct mrc_fld *fld_double)
{
  assert(mrc_fld_same_shape(fld_float, fld_double));
  assert(fld_float->_data_type == MRC_NT_FLOAT);
  assert(fld_double->_data_type == MRC_NT_DOUBLE);
  float *f_arr = fld_float->_arr;
  double *d_arr = fld_double->_arr;
  for (int i = 0; i < fld_float->_len; i++) {
    d_arr[i] = f_arr[i];
  }
}

// ----------------------------------------------------------------------
// mrc_fld_*_methods

static struct mrc_obj_method mrc_fld_double_methods[] = {
  {}
};

static struct mrc_obj_method mrc_fld_float_methods[] = {
  MRC_OBJ_METHOD("copy_to_double",   mrc_fld_float_copy_to_double),
  MRC_OBJ_METHOD("copy_from_double", mrc_fld_float_copy_from_double),
  {}
};

static struct mrc_obj_method mrc_fld_int_methods[] = {
  {}
};

// ----------------------------------------------------------------------
// create float, double, int subclasses

#define MAKE_MRC_FLD_TYPE(type, TYPE)			\
							\
  static void						\
  mrc_fld_##type##_create(struct mrc_fld *fld)		\
  {							\
    fld->_data_type = MRC_NT_##TYPE;			\
    fld->_size_of_type = sizeof(type);			\
  }							\
  							\
  static struct mrc_fld_ops mrc_fld_##type##_ops = {	\
    .name                  = #type,			\
    .methods               = mrc_fld_##type##_methods,	\
    .create                = mrc_fld_##type##_create,	\
  };							\

MAKE_MRC_FLD_TYPE(float, FLOAT)
MAKE_MRC_FLD_TYPE(double, DOUBLE)
MAKE_MRC_FLD_TYPE(int, INT)

// ----------------------------------------------------------------------
// mrc_fld_init

static void
mrc_fld_init()
{
  mrc_class_register_subclass(&mrc_class_mrc_fld, &mrc_fld_float_ops);
  mrc_class_register_subclass(&mrc_class_mrc_fld, &mrc_fld_double_ops);
  mrc_class_register_subclass(&mrc_class_mrc_fld, &mrc_fld_int_ops);
}

// ----------------------------------------------------------------------
// mrc_class_mrc_fld

#define VAR(x) (void *)offsetof(struct mrc_fld, x)
static struct param mrc_fld_descr[] = {
  { "offs"            , VAR(_offs)        , PARAM_INT_ARRAY(0, 0) },
  { "dims"            , VAR(_dims)        , PARAM_INT_ARRAY(0, 0) },
  { "sw"              , VAR(_sw)          , PARAM_INT_ARRAY(0, 0) },

  { "size_of_type"    , VAR(_size_of_type), MRC_VAR_INT           },
  { "len"             , VAR(_len)         , MRC_VAR_INT           },
  { "vec"             , VAR(_vec)         , MRC_VAR_OBJ(mrc_vec)  },
  {},
};
#undef VAR

static struct mrc_obj_method mrc_fld_methods[] = {
  MRC_OBJ_METHOD("duplicate", mrc_fld_duplicate),
  MRC_OBJ_METHOD("copy"     , mrc_fld_copy),
  MRC_OBJ_METHOD("axpy"     , mrc_fld_axpy),
  MRC_OBJ_METHOD("waxpy"    , mrc_fld_waxpy),
  MRC_OBJ_METHOD("norm"     , mrc_fld_norm),
  {}
};

// ----------------------------------------------------------------------
// mrc_fld class description

struct mrc_class_mrc_fld mrc_class_mrc_fld = {
  .name         = "mrc_fld",
  .size         = sizeof(struct mrc_fld),
  .param_descr  = mrc_fld_descr,
  .methods      = mrc_fld_methods,
  .init         = mrc_fld_init,
  .destroy      = _mrc_fld_destroy,
  .setup        = _mrc_fld_setup,
  .write        = _mrc_fld_write,
  .read         = _mrc_fld_read,
};

