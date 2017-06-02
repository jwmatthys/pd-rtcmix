#include "m_pd.h"  
#include "../rtcmix/RTcmix_API.h"
 
static t_class *rtcmix_tilde_class;  
 
typedef struct _rtcmix_tilde {  
  t_object  x_obj;  
} t_rtcmix_tilde;  
 
void rtcmix_tilde_bang(t_rtcmix_tilde *x)  
{  
  post("Hello world!");  
}  
 
void *rtcmix_tilde_new(void)  
{  
  t_rtcmix_tilde *x = (t_rtcmix_tilde *)pd_new(rtcmix_tilde_class);  
  
  return (void *)x;  
}  
 
void rtcmix_tilde_setup(void) {  
  rtcmix_tilde_class = class_new(gensym("rtcmix~"),  
        (t_newmethod)rtcmix_tilde_new,  
        0, sizeof(t_rtcmix_tilde),  
        CLASS_DEFAULT, 0);  
  class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang);  
}
