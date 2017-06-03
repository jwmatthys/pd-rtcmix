#include "m_pd.h"
#include "../rtcmix/RTcmix_API.h"
#define UNUSED(x) (void)(x)

static t_class *rtcmix_tilde_class;

typedef struct _rtcmix_tilde
{
  t_object  x_obj;
  
  //variables specific to this object
  float srate;                                        //sample rate
  short num_inputs, num_outputs;       //number of inputs and outputs
  short num_pinlets;				// number of inlets for dynamic PField control
  float in[8];			// values received for dynamic PFields
  float in_connected[8]; //booleans: true if signals connected to the input in question
  //we use this "connected" boolean so that users can connect *either* signals or floats
  //to the various inputs; sometimes it's easier just to have floats, but other times
  //it's essential to have signals.... but we have to know.
  //JWM: We'll see if this works in Pd
  t_outlet *outpointer;
  
} t_rtcmix_tilde;
 
void rtcmix_tilde_bang(t_rtcmix_tilde *x)
{
  UNUSED(x);
  post("Hello world!");
}

void *rtcmix_tilde_new(void)
{
  t_rtcmix_tilde *x = (t_rtcmix_tilde *)pd_new(rtcmix_tilde_class);
  RTcmix_init();
  return (void *)x;
}

void rtcmix_tilde_free(t_rtcmix_tilde *x)
{
  UNUSED(x);
	RTcmix_destroy();
}
 
void rtcmix_tilde_setup(void)
{
  rtcmix_tilde_class = class_new(gensym("rtcmix~"),
        (t_newmethod)rtcmix_tilde_new,
        (t_method)rtcmix_tilde_free,
        sizeof(t_rtcmix_tilde),
        CLASS_DEFAULT, 0);
  class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang);
}
