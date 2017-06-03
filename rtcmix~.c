#include "m_pd.h"
#include "rtcmix~.h"
#include "../rtcmix/RTcmix_API.h"
#define UNUSED(x) (void)(x)
 
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
