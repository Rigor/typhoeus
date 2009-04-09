#include <http_machine_easy.h>
	
static void dealloc(CurlEasy *curl_easy) {
	if (curl_easy->request_chunk != NULL) {
		free(curl_easy->request_chunk);
	}
	
	if (curl_easy->headers != NULL) {
		curl_slist_free_all(curl_easy->headers);
	}
	
	curl_easy_cleanup(curl_easy->curl);
	
	free(curl_easy);
}

static VALUE easy_setopt_string(VALUE self, VALUE opt_name, VALUE parameter) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	long opt = NUM2LONG(opt_name);
	curl_easy_setopt(curl_easy->curl, opt, StringValuePtr(parameter));
	return opt_name;
}

static VALUE easy_setopt_long(VALUE self, VALUE opt_name, VALUE parameter) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	long opt = NUM2LONG(opt_name);
	curl_easy_setopt(curl_easy->curl, opt, NUM2LONG(parameter));
	return opt_name;	
}

static VALUE easy_getinfo_string(VALUE self, VALUE info) {
	char *info_string;
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	long opt = NUM2LONG(info);
	curl_easy_getinfo(curl_easy->curl, opt, &info_string);
	
	return rb_str_new2(info_string);
}

static VALUE easy_getinfo_long(VALUE self, VALUE info) {
	long info_long;
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	long opt = NUM2LONG(info);
	curl_easy_getinfo(curl_easy->curl, opt, &info_long);
	
	return LONG2NUM(info_long);
}

static VALUE easy_getinfo_double(VALUE self, VALUE info) {
	double info_double;
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	long opt = NUM2LONG(info);
	curl_easy_getinfo(curl_easy->curl, opt, &info_double);
	
	return rb_dbl2big(info_double);
}

static VALUE easy_perform(VALUE self) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);
	curl_easy_perform(curl_easy->curl);

	return Qnil;
}

static VALUE easy_reset(VALUE self) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	curl_easy_reset(curl_easy->curl);

	if (curl_easy->request_chunk != NULL) {
		free(curl_easy->request_chunk);
	}
	
	if (curl_easy->headers != NULL) {
		curl_slist_free_all(curl_easy->headers);
	}

	return Qnil;	
}

static size_t write_data_handler(char *stream, size_t size, size_t nmemb, VALUE val) {
	rb_funcall(val, idAppend, 1, rb_str_new(stream, size * nmemb));
	return size * nmemb;
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *data) {
	int realsize = size * nmemb;
	RequestChunk *mem = (RequestChunk *)data;

	if (realsize > mem->size - mem->read) {
		realsize = mem->size - mem->read;
	}
	
	if (realsize != 0) {
		memcpy(ptr, &(mem->memory[mem->read]), realsize);
		mem->read += realsize;
	}
	
	return realsize; 
}

static VALUE easy_add_header(VALUE self, VALUE header) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	curl_easy->headers = curl_slist_append(curl_easy->headers, StringValuePtr(header));
	return header;
}

static VALUE easy_set_headers(VALUE self) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	curl_easy_setopt(curl_easy->curl, CURLOPT_HTTPHEADER, curl_easy->headers);
	
	return Qnil;
}

static VALUE easy_set_request_body(VALUE self, VALUE data, VALUE content_length_header) {
	CurlEasy *curl_easy;
	Data_Get_Struct(self, CurlEasy, curl_easy);

	curl_easy->request_chunk = ALLOC(RequestChunk);
	curl_easy->request_chunk->size = RSTRING_LEN(data);
	curl_easy->request_chunk->memory = StringValuePtr(data);
	curl_easy->request_chunk->read = 0;

  curl_easy_setopt(curl_easy->curl, CURLOPT_READFUNCTION, (curl_read_callback)read_callback);
  curl_easy_setopt(curl_easy->curl, CURLOPT_READDATA, curl_easy->request_chunk);
	curl_easy_setopt(curl_easy->curl, CURLOPT_INFILESIZE, RSTRING_LEN(data));

	return Qnil;
}

static VALUE new(int argc, VALUE *argv, VALUE klass) {
	CURL *curl = curl_easy_init();
	CurlEasy *curl_easy = ALLOC(CurlEasy);
	curl_easy->curl = curl;
	curl_easy->headers = NULL;
	curl_easy->request_chunk = NULL;
	VALUE easy = Data_Wrap_Struct(cHTTPMachineEasy, 0, dealloc, curl_easy);
	
	rb_iv_set(easy, "@response_body", rb_str_new2(""));
	rb_iv_set(easy, "@response_header", rb_str_new2(""));
	
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)&write_data_handler);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, rb_iv_get(easy, "@response_body"));	
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)&write_data_handler);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, rb_iv_get(easy, "@response_header"));

	rb_obj_call_init(easy, argc, argv);

	return easy;
}

void init_http_machine_easy() {
	VALUE klass = cHTTPMachineEasy = rb_define_class_under(mHTTPMachine, "Easy", rb_cObject);
	idAppend = rb_intern("<<");
	rb_define_singleton_method(klass, "new", new, -1);
	rb_define_private_method(klass, "easy_setopt_string", easy_setopt_string, 2);
	rb_define_private_method(klass, "easy_setopt_long", easy_setopt_long, 2);
	rb_define_private_method(klass, "easy_getinfo_string", easy_getinfo_string, 1);
	rb_define_private_method(klass, "easy_getinfo_long", easy_getinfo_long, 1);
	rb_define_private_method(klass, "easy_getinfo_double", easy_getinfo_double, 1);
	rb_define_private_method(klass, "easy_perform", easy_perform, 0);
	rb_define_private_method(klass, "easy_reset", easy_reset, 0);
	rb_define_private_method(klass, "easy_set_request_body", easy_set_request_body, 1);
	rb_define_private_method(klass, "easy_set_headers", easy_set_headers, 0);
	rb_define_private_method(klass, "easy_add_header", easy_add_header, 1);
}