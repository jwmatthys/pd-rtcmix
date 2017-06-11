#include "m_pd.h"
#include "m_imp.h"
#include "rtcmix~.h"
#include "../rtcmix/RTcmix_API.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>

#define UNUSED(x) (void)(x)

#define DEBUG(x) x // debug on
//#define DEBUG(x) // debug off

void rtcmix_tilde_setup(void)
{
        rtcmix_tilde_class = class_new (gensym("rtcmix~"),
                                        (t_newmethod)rtcmix_tilde_new,
                                        (t_method)rtcmix_tilde_free,
                                        sizeof(t_rtcmix_tilde),
                                        0, A_GIMME, 0);

        //standard messages; don't change these
        CLASS_MAINSIGNALIN(rtcmix_tilde_class, t_rtcmix_tilde, f);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_tilde_dsp, gensym("dsp"), 0);

        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_version, gensym("version"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_info, gensym("info"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_verbose, gensym("verbose"), A_FLOAT, 0);

        class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang); // trigger scripts
        class_addfloat(rtcmix_tilde_class, rtcmix_tilde_float);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_openeditor, gensym("click"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_editor, gensym("editor"), A_SYMBOL, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_var, gensym("var"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_varlist, gensym("varlist"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_flush, gensym("flush"), 0);

        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("read"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("load"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("open"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("write"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("save"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("saveas"), A_GIMME, 0);
        // openpanel and savepanel return their messages through "callback"
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_callback, gensym("callback"), A_SYMBOL, 0);

        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_setscript, gensym("setscript"), A_FLOAT, 0);
        //our own messages
        //so we know what to do with floats that we receive at the inputs
/*
   class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("save"), 0);
   class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("save"), 0);
   class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("write"), 0);
   class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_bufset, gensym("bufset"), A_SYMBOL, 0);
   class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_callback, gensym("callback"), A_SYMBOL, 0);
 */
// Hackity hack hack hack!
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp9, gensym("pinlet9"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp8, gensym("pinlet8"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp7, gensym("pinlet7"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp6, gensym("pinlet6"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp5, gensym("pinlet5"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp4, gensym("pinlet4"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp3, gensym("pinlet3"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp2, gensym("pinlet2"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp1, gensym("pinlet1"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp0, gensym("pinlet0"), A_FLOAT, 0);
}


void *rtcmix_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *)pd_new(rtcmix_tilde_class);
        UNUSED(s);
        null_the_pointers(x);
        RTcmix_init();
        short num_inoutputs = 1;
        short num_additional = 0;
        // JWM: add optional third argument to autoload scorefile
        t_symbol* optional_filename = NULL;

        RTcmix_setBangCallback(rtcmix_bangcallback, x);
        RTcmix_setValuesCallback(rtcmix_valuescallback, x);
        RTcmix_setPrintCallback(rtcmix_printcallback, x);

        int this_arg;
        int float_arg = 0;
        for (this_arg=0; this_arg<argc; this_arg++)
        {
                switch (argv[this_arg].a_type)
                {
                case A_SYMBOL:
                        optional_filename = argv[this_arg].a_w.w_symbol;
                        //post("rtcmix~: instantiating with scorefile %s",optional_filename->s_name);
                        break;
                case A_FLOAT:
                        if (float_arg == 0)
                        {
                                num_inoutputs = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d signal inlets and outlets",num_inoutputs); );
                                float_arg++;
                        }
                        else if (float_arg == 1)
                        {
                                num_additional = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d pfield inlets",num_additional); );
                        }
                default:
                {}
                }
        }
        //DEBUG(post("creating %d inlets and outlets and %d additional inlets",num_inoutputs,num_additional););
        if (num_inoutputs < 1) num_inoutputs = 1; // no args, use default of 1 channel in/out
        if ((num_inoutputs + num_additional) > MAX_INPUTS)
        {
                num_inoutputs = 1;
                num_additional = 0;
                error("sorry, only %d total inlets are allowed!", MAX_INPUTS);
        }
        // JWM: limiting num_additional to 10
        if (num_additional > 10)
                num_additional = 10;

        x->num_inputs = num_inoutputs;
        x->num_outputs = num_inoutputs;
        x->num_pinlets = num_additional;

        // setup up inputs and outputs, for audio inputs
        int i;

        // SIGNAL INLETS
        for (i=0; i < x->num_inputs-1; i++)
                inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);

        // FLOAT INLETS (for pfields)
        // JWM: This is a hack, since I couldn't find a way to identify a float
        // passed to a single method by inlet id. So I'm limiting the number of
        // inlets to 10 (not sure why we'd need more anyway) and passing each to
        // gensym "pinlet0", "pinlet1", etc.
        char* inletname = malloc(8);
        x->pfieldinlets = malloc(x->num_pinlets);
        x->pfield_in = malloc(x->num_pinlets);
        for (i=0; i< x->num_pinlets; i++)
        {
                sprintf(inletname, "pinlet%d", i);
                x->pfieldinlets[i] = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym(inletname));
                x->pfield_in[i] = 0.0;
        }
        free(inletname);

        // SIGNAL OUTLETS
        x->signaloutlets = malloc(x->num_outputs);
        for (i = 0; i < x->num_outputs; i++)
        {
                // outputs, right-to-left
                x->signaloutlets[i] = outlet_new(&x->x_obj, gensym("signal"));
        }

        // OUTLET FOR BANGS
        x->outpointer = outlet_new(&x->x_obj, &s_bang);

        // initialize some variables; important to do this!
        for (i = 0; i < (x->num_inputs + x->num_pinlets); i++)
        {
                x->pfield_in[i] = 0.;
        }

        // set up for the variable-substitution scheme
        x->var_array = malloc(NVARS * sizeof(float));
        x->var_set = malloc(NVARS * sizeof(bool));
        for(i = 0; i < NVARS; i++)
        {
                x->var_set[i] = false;
                x->var_array[i] = 0.0;
        }
        // the text editor

        char *sys_cmd = malloc(MAXPDSTRING);
        x->tempfolder_path = malloc(MAXPDSTRING);
        sprintf(x->tempfolder_path,"/tmp/rtcmixXXXXXX");
        // create temp folder
        x->tempfolder_path = mkdtemp(x->tempfolder_path);
        // create unique name for dylib
        DEBUG(post("rtcmix~: tempfolder: %s", x->tempfolder_path); );
        // allow other users to read and write (no execute tho)
        sprintf(sys_cmd, "chmod 766 %s", x->tempfolder_path);
        if (system(sys_cmd))
                error ("rtcmix~: error setting temp folder \"%s\" permissions.", x->tempfolder_path);

        x->externdir = rtcmix_tilde_class->c_externdir->s_name;
        x->editorpath = malloc(MAXPDSTRING);
        DEBUG(post("dir: %s %i %i",x->externdir, strlen(x->externdir),MAXPDSTRING); );
        sprintf(x->editorpath, "python \"%s/%s\"", x->externdir, "rtcmix_editor.py");
        char buf[50];
        sprintf(buf, "d%lx", (t_int)x);
        x->x_s = gensym(buf);
        pd_bind(&x->x_obj.ob_pd, x->x_s);

        DEBUG(post("rtcmix~: editor_path: %s", x->editorpath); );

        x->current_script = 0;

        x->rtcmix_script = malloc(MAX_SCRIPTS * MAXSCRIPTSIZE);
        x->tempscript_path = malloc(MAX_SCRIPTS * MAXPDSTRING);

        for (i=0; i<MAX_SCRIPTS; i++)
        {
                x->rtcmix_script[i] = malloc(MAXSCRIPTSIZE);
                x->tempscript_path[i] = malloc(MAXPDSTRING);
                sprintf(x->tempscript_path[i],"%s/%s%i.%s",x->tempfolder_path, TEMPFILENAME, i, SCOREEXTENSION);
                //DEBUG(post(x->tempscript_path[i],"%s/%s%i.%s",x->tempfolder_path, TEMPFILENAME, i, SCOREEXTENSION););
        }

        x->x_canvas = canvas_getcurrent();
        x->canvas_path = canvas_getdir(x->x_canvas);

        x->flushflag = false;
        x->verbose = debug;
        x->rw_flag = none;

        post("rtcmix~ --- RTcmix music language, http://rtcmix.org ---");
        post("rtcmix~ version%s by Joel Matthys", VERSION);

        // If filename is given in score instantiation, open scorefile
        if (optional_filename)
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, optional_filename->s_name, fullpath, MAXPDSTRING);
                //sprintf(x->tempscript_path[x->current_script], "%s", fullpath);
                DEBUG( post ("opening scorefile %s", optional_filename->s_name); );
                //post("default scorefile: %s", default_scorefile->s_name);
                rtcmix_read(x, fullpath);
                free(fullpath);
        }
        free(sys_cmd);
        x->rw_flag = none;
        x->buffer_changed = false;
        return (void *)x;
}

void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp)
{
        // This is 2 because (for some totally crazy reason) the
        // signal vector starts at 1, not 0
        t_int dsp_add_args [x->num_inputs + x->num_outputs + 2];
        t_int vector_size = sp[0]->s_n;
        int i;
        x->srate = sys_getsr();

        // construct the array of vectors and stuff
        dsp_add_args[0] = (t_int)x; //the object itself
        for(i = 0; i < (x->num_inputs + x->num_outputs); i++)
        { //pointers to the input and output vectors
                dsp_add_args[i+1] = (t_int)sp[i]->s_vec;
        }

        dsp_add_args[x->num_inputs + x->num_outputs + 1] = vector_size; //pointer to the vector size

        DEBUG(post("vector size: %d",vector_size); );

        dsp_addv(rtcmix_tilde_perform, (x->num_inputs  + x->num_outputs + 2),(t_int*)dsp_add_args);//add them to the signal chain

// allocate the RTcmix i/o transfer buffers
        x->pd_inbuf = malloc(sizeof(float) * vector_size * x->num_inputs);
        x->pd_outbuf = malloc(sizeof(float) * vector_size * x->num_outputs);

        // zero out these buffers for UB
        for (i = 0; i < (vector_size * x->num_inputs); i++) x->pd_inbuf[i] = 0.0;
        for (i = 0; i < (vector_size * x->num_outputs); i++) x->pd_outbuf[i] = 0.0;

        DEBUG(post("x->srate: %f, x->num_outputs: %d, vector_size %d, 1, 0", x->srate, x->num_outputs, vector_size); );
        RTcmix_setAudioBufferFormat(AudioFormat_32BitFloat_Normalized, x->num_outputs);
        RTcmix_setparams(x->srate, x->num_outputs, vector_size, 1, 0);

}

t_int *rtcmix_tilde_perform(t_int *w)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *)(w[1]);
        t_int vecsize = w[x->num_inputs + x->num_outputs + 2]; //number of samples per vector
        float *in[x->num_inputs]; //pointers to the input vectors
        float *out[x->num_outputs]; //pointers to the output vectors

        int i = x->num_outputs * vecsize;
        //while (i--) out[i] = (float *)0.;

        checkForBang();
        checkForVals();
        checkForPrint();

        // reset queue and heap if signalled
        if (x->flushflag == true)
        {
                RTcmix_flushScore();
                x->flushflag = false;
        }

        //post ("samples_per_vector: %d", vecsize);

        int j, k;

        for (i = 0; i < (x->num_inputs + x->num_pinlets); i++)
        {
                in[i] = (float *)(w[i+2]);
        }

        //assign the output vectors
        for (i = 0; i < x->num_outputs; i++)
        {
                // this results in reversed L-R image but I'm
                // guessing it's the same in Max
                out[i] = (float *)( w[x->num_inputs + i + 2 ]);
        }

        j = 0;
        k = 0;

        for (k = 0; k < vecsize; k++)
        {
                for(i = 0; i < x->num_inputs; i++)
                        (x->pd_inbuf)[k++] = *in[i]++;
        }

        RTcmix_runAudio (x->pd_inbuf, x->pd_outbuf, 1);

        for (k = 0; k < vecsize; k++)
        {
                for(i = 0; i < x->num_outputs; i++)
                        *out[i]++ = (x->pd_outbuf)[j++];
        }

        return w + x->num_inputs + x->num_outputs + 3;
}

void rtcmix_tilde_free(t_rtcmix_tilde *x)
{
        int i;

        for (i=0; i<MAX_SCRIPTS; i++)
        {
                free(x->rtcmix_script[i]);
                free(x->tempscript_path[i]);
        }

        free(x->canvas_path);
        free(x->pd_inbuf);
        free(x->pd_outbuf);
        free(x->var_array);
        free(x->var_set);
        free(x->rtcmix_script);
        free(x->tempscript_path);
        free(x->editorpath);
        free(x->externdir);
        outlet_free(x->outpointer);
        for (i=0; i< x->num_pinlets; i++) inlet_free(x->pfieldinlets[i]);
        for (i=0; i< x->num_inputs; i++) inlet_free(x->signalinlets[i]);
        for (i=0; i< x->num_outputs; i++) outlet_free(x->signaloutlets[i]);

        //binbuf_free(x->x_binbuf);
        DEBUG(post ("rtcmix~ DESTROYED!"); );
        RTcmix_destroy();
}

// print out the rtcmix~ version
void rtcmix_version(t_rtcmix_tilde *x)
{
        post("rtcmix~, v. %s by Joel Matthys", VERSION);
        post("compiled at %s on %s",__TIME__, __DATE__);
        outlet_bang(x->outpointer);
}

void rtcmix_info(t_rtcmix_tilde *x)
{
        rtcmix_version(x);
}

// the "var" message allows us to set $n variables imbedded in a scorefile with varnum value messages
void rtcmix_var(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        short i, varnum;
        UNUSED(s);

        for (i = 0; i < argc; i += 2)
        {
                varnum = (short)argv[i].a_w.w_float;
                if ( (varnum < 1) || (varnum > NVARS) )
                {
                        error("only vars $1 - $9 are allowed");
                        return;
                }
                x->var_set[varnum-1] = true;
                if (argv[i+1].a_type == A_FLOAT)
                        x->var_array[varnum-1] = argv[i+1].a_w.w_float;
        }
        DEBUG(post("vars: %f %f %f %f %f %f %f %f %f",
                   (float)(x->var_array[0]),
                   (float)(x->var_array[1]),
                   (float)(x->var_array[2]),
                   (float)(x->var_array[3]),
                   (float)(x->var_array[4]),
                   (float)(x->var_array[5]),
                   (float)(x->var_array[6]),
                   (float)(x->var_array[7]),
                   (float)(x->var_array[8])); );
}


// the "varlist" message allows us to set $n variables imbedded in a scorefile with a list of positional vars
void rtcmix_varlist(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        short i;
        UNUSED(s);

        if (argc > NVARS)
        {
                error("asking for too many variables, only setting the first 9 ($1-$9)");
                argc = NVARS;
        }

        for (i = 0; i < argc; i++)
        {
                x->var_set[i] = true;
                if (argv[i].a_type == A_FLOAT)
                        x->var_array[i] = argv[i].a_w.w_float;
        }
}

// the "flush" message
void rtcmix_flush(t_rtcmix_tilde *x)
{
        DEBUG( post("flushing"); );
        if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0) return;
        x->flushflag = true;
}

// bang triggers the current working script
void rtcmix_tilde_bang(t_rtcmix_tilde *x)
{
        DEBUG(post("rtcmix~: received bang"); );

        if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0) return;
        RTcmix_parseScore(x->rtcmix_script[x->current_script], strlen(x->rtcmix_script[x->current_script]));

        //rtcmix_goscript(x, x->current_script);
}
void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum)
{
        DEBUG(post("received float %f",scriptnum); );
        if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0) return;
        //TODO: bounds check
        RTcmix_parseScore(x->rtcmix_script[(int)scriptnum], strlen(x->rtcmix_script[(int)scriptnum]));
}

void rtcmix_float_inlet(t_rtcmix_tilde *x, short inlet, t_float f)
{
        //check to see which input the float came in, then set the appropriate variable value
        //TODO: bounds check
        if (inlet >= x->num_pinlets)
        {
                x->pfield_in[inlet] = f;
                post("rtcmix~: setting in[%d] =  %f, but rtcmix~ doesn't use this", inlet, f);
        }
        else RTcmix_setPField(inlet+1, f);
}

void rtcmix_inletp0(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 0",f); );
        rtcmix_float_inlet(x,0,f);
}

void rtcmix_inletp1(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 1",f); );
        rtcmix_float_inlet(x,1,f);
}
void rtcmix_inletp2(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 2",f); );
        rtcmix_float_inlet(x,2,f);
}
void rtcmix_inletp3(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 3",f); );
        rtcmix_float_inlet(x,3,f);
}
void rtcmix_inletp4(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 4",f); );
        rtcmix_float_inlet(x,4,f);
}
void rtcmix_inletp5(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 5",f); );
        rtcmix_float_inlet(x,5,f);
}
void rtcmix_inletp6(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 6",f); );
        rtcmix_float_inlet(x,6,f);
}
void rtcmix_inletp7(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 7",f); );
        rtcmix_float_inlet(x,7,f);
}
void rtcmix_inletp8(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 8",f); );
        rtcmix_float_inlet(x,8,f);
}
void rtcmix_inletp9(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 9",f); );
        rtcmix_float_inlet(x,9,f);
}

void rtcmix_verbose (t_rtcmix_tilde *x, t_float f)
{
        switch ((short)f)
        {
        case 0:
                x->verbose = silent;
                break;
        case 2:
                x->verbose = debug;
                break;
        case 1:
        default:
                x->verbose = normal;
        }
        post("rtcmix~: verbosity set to %i",(short)f);
}

void rtcmix_openeditor(t_rtcmix_tilde *x)
{
        DEBUG( post ("clicked."); );
        x->buffer_changed = true;
        sys_vgui("exec %s %s &\n",x->editorpath, x->tempscript_path[x->current_script]);
}

void rtcmix_editor (t_rtcmix_tilde *x, t_symbol *s)
{
        char *str = s->s_name;
        if (0==strcmp(str,"tedit")) sprintf(x->editorpath, "\"%s/%s\"", x->externdir, "tedit/tedit");
        else if (0==strcmp(str,"rtcmix")) sprintf(x->editorpath, "python \"%s/%s\"", x->externdir, "rtcmix_editor.py");
        else sprintf(x->editorpath, "\"%s\"", str);
        post("setting the text editor to %s",str);
}

void rtcmix_setscript(t_rtcmix_tilde *x, t_float s)
{
        DEBUG(post("setscript: %d", (int)s); );
        if (s >= 0 && s < MAX_SCRIPTS)
        {
                DEBUG(post ("changed current script to %d", (int)s); );
                x->current_script = (int)s;
        }
}

void rtcmix_read(t_rtcmix_tilde *x, char* fullpath)
{
        DEBUG( post("read %s",fullpath); );
        FILE *fp = fopen ( fullpath, "r" );
        long lSize = 0;
        char buffer [MAXSCRIPTSIZE];
        if( fp == NULL )
        {
                error("rtcmix~: error reading \"%s\"", fullpath);
                if (fp) fclose(fp);
                return;
        }

        fseek( fp, 0L, SEEK_END);
        lSize = ftell( fp );
        rewind( fp );

        if (lSize>MAXSCRIPTSIZE)
        {
                error("rtcmix~: error: file is longer than MAXSCRIPTSIZE");
                if (fp) fclose(fp);
                return;
        }

        post("rtcmix~: read \"%s\"", fullpath);

        if( fread( buffer, lSize, 1, fp) != 1 )
        {
                // error if file is empty; this is not necessary an error
                // if you close the editor with an empty file
                error("rtcmix~: failed to read file");
        }

        if (lSize>0)
                sprintf(x->rtcmix_script[x->current_script], "%s",buffer);

        sprintf(x->tempscript_path[x->current_script], "%s", fullpath);

        if (fp) fclose(fp);

}

void rtcmix_write(t_rtcmix_tilde *x, char* filename)
{
        post("write %s", filename);
        char * sys_cmd = malloc(MAXPDSTRING);
        sprintf(sys_cmd, "cp \"%s\" \"%s\"", x->tempscript_path[x->current_script], filename);
        if (system(sys_cmd)) error ("rtcmix~: error saving %s",filename);
        else
        if (x->verbose != silent)
                post("rtcmix~: wrote script %i to %s",x->current_script,filename);
        sprintf(x->tempscript_path[x->current_script], "%s", filename);
        free(sys_cmd);
        /*
           FILE *fp;
           fp = fopen (filename, "w");
           if( fp == NULL )
           {
           error("rtcmix~: error opening \"%s\" for writing", filename);
           }
           else
           {
           const char* buf = x->rtcmix_script[x->current_script];
           fputs(buf, fp);
             sprintf(x->tempscript_path[x->current_script], "%s", filename);
           }
           if (fp) fclose(fp);*/
}

void rtcmix_open(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);
        if (argc == 0)
        {
                x->rw_flag = read;
                DEBUG( post ("pdtk_openpanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name); );
                sys_vgui("pdtk_openpanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name);
        }
        else
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, argv[0].a_w.w_symbol->s_name, fullpath, MAXPDSTRING);
                rtcmix_read(x, fullpath);
                free (fullpath);
        }
}

void rtcmix_save(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);
        if (argc == 0)
        {
                x->rw_flag = write;
                DEBUG( post ("pdtk_savepanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name); );
                sys_vgui("pdtk_savepanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name);
        }
        else
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, argv[0].a_w.w_symbol->s_name, fullpath, MAXPDSTRING);
                rtcmix_write(x, fullpath);
                free (fullpath);
        }
}

void rtcmix_callback (t_rtcmix_tilde *x, t_symbol *s)
{
        switch (x->rw_flag)
        {
        case read:
                post("read %s", s->s_name);
                rtcmix_read(x, s->s_name);
                break;
        case write:
                post("write %s", s->s_name);
                rtcmix_write(x, s->s_name);
                break;
        case none:
                post("none %s (this shouldn't happen)", s->s_name);
                break;
        default:
                post("default %s (this shouldn't happen)", s->s_name);
        }
        x->rw_flag = none;
}

void rtcmix_bangcallback(void *inContext)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *) inContext;
        // got a pending bang from MAXBANG()
        outlet_bang(x->outpointer);
        //defer_low(x, (method)rtcmix_dobangout, (Symbol *)NULL, 0, (Atom *)NULL);
}

void rtcmix_valuescallback(float *values, int numValues, void *inContext)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *) inContext;
        int i;
        // BGG -- I should probably defer this one and the error posts also.  So far not a problem...
        if (numValues == 1)
                outlet_float(x->outpointer, (double)(values[0]));
        else {
                for (i = 0; i < numValues; i++) SETFLOAT((x->valslist)+i, values[i]);
                outlet_list(x->outpointer, 0L, numValues, x->valslist);
        }
}

void rtcmix_printcallback(const char *printBuffer, void *inContext)
{
        UNUSED(inContext);
        const char *pbufptr = printBuffer;
        while (strlen(pbufptr) > 0) {
                post("RTcmix: %s", pbufptr);
                pbufptr += (strlen(pbufptr) + 1);
        }
}

void null_the_pointers(t_rtcmix_tilde *x)
{
        x->pfield_in = NULL;
        x->outpointer = NULL;
        x->signalinlets = NULL;
        x->signaloutlets = NULL;
        x->pfieldinlets = NULL;
        x->tempfolder_path = NULL;
        x->pd_inbuf = NULL;
        x->pd_outbuf = NULL;
        x->restore_buf_ptr = NULL;
        x->var_array = NULL;
        x->var_set = NULL;

        x->rtcmix_script = NULL;
        x->tempscript_path = NULL;
        x->x_canvas = NULL;
        x->x_guiconnect = NULL;
        x->canvas_path = NULL;
        x->x_s = NULL;
        x->editorpath = NULL;
        x->externdir = NULL;
}
