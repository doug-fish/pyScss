/*
* pyScss, a Scss compiler for Python
* SCSS blocks scanner.
*
* German M. Bravo (Kronuz) <german.mb@gmail.com>
* https://github.com/Kronuz/pyScss
*
* MIT license (http://www.opensource.org/licenses/mit-license.php)
* Copyright (c) 2011 German M. Bravo (Kronuz), All rights reserved.
*/
#include <Python.h>

/* Counter type */
staticforward PyTypeObject scss_BlockLocatorType;

#undef DEBUG

void reprl(char *str, int len) {
	char c,
		 *begin = str,
		 *end = str + len;
	PySys_WriteStderr("'");
	while (begin < end) {
		c = *begin;
		if (len == -1 && !c) {
			break;
		} else if (c == '\r') {
			PySys_WriteStderr("\\r");
		} else if (c == '\n') {
			PySys_WriteStderr("\\n");
		} else if (c == '\t') {
			PySys_WriteStderr("\\t");
		} else if (c < ' ') {
			PySys_WriteStderr("\\x%02x", c);
		} else {
			PySys_WriteStderr("%c", c);
		}
		begin++;
	}
	PySys_WriteStderr("'\n");
}
void repr(char *str, char *str2) {
	reprl(str, (int)(str2 - str));
}

typedef struct {
	int error;
	int lineno;
	char *selprop;
	int selprop_sz;
	char *codestr;
	int codestr_sz;
} scss_Block;

typedef struct {
	PyObject_HEAD
	char *exc;
	char *_codestr;
	char *codestr;
	char *codestr_ptr;
	int codestr_sz;
	int lineno;
	int par;
	char instr;
	int depth;
	int skip;
	char *thin;
	char *init;
	char *safe;
	char *lose;
	char *start;
	char *end;
	scss_Block block;
} scss_BlockLocator;


int _strip(char *begin, char *end, int *lineno) {
	// "    1\0     some,    \n  2\0 aca  "
	int _cnt,
		cnt = 0,
		pass = 1,
		addnl = 0;
	char c,
		*line = NULL,
		*first = begin,
		*last = begin,
		*write = lineno ? begin : NULL;
	while (begin < end) {
		c = *begin;
		if (c == '\0') {
			if (line == NULL) {
				line = first;
				if (lineno) {
					sscanf(line, "%d", lineno);
				}
			}
			first = last = begin + 1;
			pass = 1;
		} else if (c == '\n') {
			_cnt = (int)(last - first);
			if (_cnt > 0) {
				cnt += _cnt + addnl;
				if (write != NULL) {
					if (addnl) {
						*write++ = '\n';
					}
					while (first < last) {
						*write++ = *first++;
					}
					*write = '\0';
					addnl = 1;
				}
			}
			first = last = begin + 1;
			pass = 1;
		} else if (c == ' ' || c == '\t') {
			if (pass) {
				first = last = begin + 1;
			}
		} else {
			last = begin + 1;
			pass = 0;
		}
		begin++;
	}
	_cnt = (int)(last - first);
	if (_cnt > 0) {
		cnt += _cnt + addnl;
		if (write != NULL) {
			if (addnl) {
				*write++ = '\n';
			}
			while (first < last) {
				*write++ = *first++;
			}
			*write = '\0';
		}
	}

	return cnt;
}


typedef void scss_Callback(scss_BlockLocator*);


void _start_string(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_start_string\n");
	#endif
	// A string starts
	self->instr = *(self->codestr_ptr);
}

void _end_string(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_end_string\n");
	#endif
	// A string ends (FIXME: needs to accept escaped characters)
	self->instr = 0;
}

void _start_parenthesis(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_start_parenthesis\n");
	#endif
	// parenthesis begins:
	self->par++;
	self->thin = NULL;
	self->safe = self->codestr_ptr + 1;
}

void _end_parenthesis(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_end_parenthesis\n");
	#endif
	self->par--;
}

void _flush_properties(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_flush_properties\n");
	#endif
	// Flush properties
	int len, lineno = -1;
	if (self->lose <= self->init) {
		len = _strip(self->lose, self->init, &lineno);
		if (len) {
			if (lineno != -1) {
				self->lineno = lineno;
			}

			self->block.selprop = self->lose;
			self->block.selprop_sz = len;
			self->block.codestr = NULL;
			self->block.codestr_sz = 0;
			self->block.lineno = self->lineno;
			self->block.error = -1;
		}
		self->lose = self->init;
	}
}

void _start_block1(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_start_block1\n");
	#endif
	// Start block:
	if (self->codestr_ptr > self->codestr && *(self->codestr_ptr - 1) == '#') {
		self->skip = 1;
	} else {
		self->start = self->codestr_ptr;
		if (self->thin != NULL && _strip(self->thin, self->codestr_ptr, NULL)) {
			self->init = self->thin;
		}
		_flush_properties(self);
		self->thin = NULL;
	}
	self->depth++;
}

void _start_block(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_start_block\n");
	#endif
	// Start block:
	self->depth++;
}

void _end_block1(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_end_block1\n");
	#endif
	// Block ends:
	int len, lineno = -1;
	self->depth--;
	if (!self->skip) {
		self->end = self->codestr_ptr;
		len = _strip(self->init, self->start, &lineno);
		if (lineno != -1) {
			self->lineno = lineno;
		}

		self->block.selprop = self->init;
		self->block.selprop_sz = len;
		self->block.codestr = (self->start + 1);
		self->block.codestr_sz = (int)(self->end - (self->start + 1));
		self->block.lineno = self->lineno;
		self->block.error = -1;

		self->init = self->safe = self->lose = self->end + 1;
		self->thin = NULL;
	}
	self->skip = 0;
}

void _end_block(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_end_block\n");
	#endif
	// Block ends:
	self->depth--;
}

void _end_property(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_end_property\n");
	#endif
	// End of property (or block):
	int len, lineno = -1;
	self->init = self->codestr_ptr;
	if (self->lose <= self->init) {
		len = _strip(self->lose, self->init, &lineno);
		if (len) {
			if (lineno != -1) {
				self->lineno = lineno;
			}

			self->block.selprop = self->lose;
			self->block.selprop_sz = len;
			self->block.codestr = NULL;
			self->block.codestr_sz = 0;
			self->block.lineno = self->lineno;
			self->block.error = -1;
		}
		self->init = self->safe = self->lose = self->codestr_ptr + 1;
	}
	self->thin = NULL;
}

void _mark_safe(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_mark_safe\n");
	#endif
	// We are on a safe zone
	if (self->thin != NULL && _strip(self->thin, self->codestr_ptr, NULL)) {
		self->init = self->thin;
	}
	self->thin = NULL;
	self->safe = self->codestr_ptr + 1;
}

void _mark_thin(scss_BlockLocator *self) {
	#ifdef DEBUG
		PySys_WriteStderr("_mark_thin\n");
	#endif
	// Step on thin ice, if it breaks, it breaks here
	if (self->thin != NULL && _strip(self->thin, self->codestr_ptr, NULL)) {
		self->init = self->thin;
		self->thin = self->codestr_ptr + 1;
	} else if (self->thin == NULL && _strip(self->safe, self->codestr_ptr, NULL)) {
		self->thin = self->codestr_ptr + 1;
	}
}

int scss_function_map_initialized = 0;
scss_Callback* scss_function_map[256 * 256 * 2 * 3]; // (c, instr, par, depth)
void init_function_map(void) {
	int i;

	if (scss_function_map_initialized)
		return;
	scss_function_map_initialized = 1;

	for (i = 0; i < 256 * 256 * 2 * 3; i++) {
		scss_function_map[i] = NULL;
	}
	scss_function_map[(int)'\"' + 256*0 + 256*256*0 + 256*256*2*0] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*0 + 256*256*2*0] = _start_string;
	scss_function_map[(int)'\"' + 256*0 + 256*256*1 + 256*256*2*0] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*1 + 256*256*2*0] = _start_string;
	scss_function_map[(int)'\"' + 256*0 + 256*256*0 + 256*256*2*1] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*0 + 256*256*2*1] = _start_string;
	scss_function_map[(int)'\"' + 256*0 + 256*256*1 + 256*256*2*1] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*1 + 256*256*2*1] = _start_string;
	scss_function_map[(int)'\"' + 256*0 + 256*256*0 + 256*256*2*2] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*0 + 256*256*2*2] = _start_string;
	scss_function_map[(int)'\"' + 256*0 + 256*256*1 + 256*256*2*2] = _start_string;
	scss_function_map[(int)'\'' + 256*0 + 256*256*1 + 256*256*2*2] = _start_string;

	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*0 + 256*256*2*0] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*0 + 256*256*2*0] = _end_string;
	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*1 + 256*256*2*0] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*1 + 256*256*2*0] = _end_string;
	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*0 + 256*256*2*1] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*0 + 256*256*2*1] = _end_string;
	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*1 + 256*256*2*1] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*1 + 256*256*2*1] = _end_string;
	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*0 + 256*256*2*2] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*0 + 256*256*2*2] = _end_string;
	scss_function_map[(int)'\"' + 256*(int)'\"' + 256*256*1 + 256*256*2*2] = _end_string;
	scss_function_map[(int)'\'' + 256*(int)'\'' + 256*256*1 + 256*256*2*2] = _end_string;

	scss_function_map[(int)'(' + 256*0 + 256*256*0 + 256*256*2*0] = _start_parenthesis;
	scss_function_map[(int)'(' + 256*0 + 256*256*1 + 256*256*2*0] = _start_parenthesis;
	scss_function_map[(int)'(' + 256*0 + 256*256*0 + 256*256*2*1] = _start_parenthesis;
	scss_function_map[(int)'(' + 256*0 + 256*256*1 + 256*256*2*1] = _start_parenthesis;
	scss_function_map[(int)'(' + 256*0 + 256*256*0 + 256*256*2*2] = _start_parenthesis;
	scss_function_map[(int)'(' + 256*0 + 256*256*1 + 256*256*2*2] = _start_parenthesis;

	scss_function_map[(int)')' + 256*0 + 256*256*1 + 256*256*2*0] = _end_parenthesis;
	scss_function_map[(int)')' + 256*0 + 256*256*1 + 256*256*2*1] = _end_parenthesis;
	scss_function_map[(int)')' + 256*0 + 256*256*1 + 256*256*2*2] = _end_parenthesis;

	scss_function_map[(int)'{' + 256*0 + 256*256*0 + 256*256*2*0] = _start_block1;
	scss_function_map[(int)'{' + 256*0 + 256*256*0 + 256*256*2*1] = _start_block;
	scss_function_map[(int)'{' + 256*0 + 256*256*0 + 256*256*2*2] = _start_block;

	scss_function_map[(int)'}' + 256*0 + 256*256*0 + 256*256*2*1] = _end_block1;
	scss_function_map[(int)'}' + 256*0 + 256*256*0 + 256*256*2*2] = _end_block;

	scss_function_map[(int)';' + 256*0 + 256*256*0 + 256*256*2*0] = _end_property;

	scss_function_map[(int)',' + 256*0 + 256*256*0 + 256*256*2*0] = _mark_safe;

	scss_function_map[(int)'\n' + 256*0 + 256*256*0 + 256*256*2*0] = _mark_thin;

	scss_function_map[0 + 256*0 + 256*256*0 + 256*256*2*0] = _flush_properties;
	scss_function_map[0 + 256*0 + 256*256*0 + 256*256*2*1] = _flush_properties;
	scss_function_map[0 + 256*0 + 256*256*0 + 256*256*2*2] = _flush_properties;
	#ifdef DEBUG
		PySys_WriteStderr("Scss function maps initialized!\n");
	#endif
}


/* constructor */

static PyObject *
scss_BlockLocator_new(char *codestr, int codestr_sz)
{
	scss_BlockLocator *self;

	init_function_map();

	self = PyObject_New(scss_BlockLocator, &scss_BlockLocatorType);;
	if (self) {
		self->_codestr = (char *)malloc(codestr_sz);
		memcpy(self->_codestr, codestr, codestr_sz);
		self->codestr_sz = codestr_sz;
		self->codestr = (char *)malloc(self->codestr_sz);
		memcpy(self->codestr, self->_codestr, self->codestr_sz);
		self->codestr_ptr = self->codestr;
		self->lineno = 0;
		self->par = 0;
		self->instr = 0;
		self->depth = 0;
		self->skip = 0;
		self->thin = self->codestr;
		self->init = self->codestr;
		self->safe = self->codestr;
		self->lose = self->codestr;
		self->start = NULL;
		self->end = NULL;
		self->exc = NULL;

		#ifdef DEBUG
			PySys_WriteStderr("Scss BlockLocator object initialized! (%lu)\n", sizeof(scss_BlockLocator));
		#endif
	}

	return (PyObject *)self;
}

static void
scss_BlockLocator_rewind(scss_BlockLocator *self)
{
	free(self->codestr);
	self->codestr = (char *)malloc(self->codestr_sz);
	memcpy(self->codestr, self->_codestr, self->codestr_sz);
	self->codestr_ptr = self->codestr;
	self->lineno = 0;
	self->par = 0;
	self->instr = 0;
	self->depth = 0;
	self->skip = 0;
	self->thin = self->codestr;
	self->init = self->codestr;
	self->safe = self->codestr;
	self->lose = self->codestr;
	self->start = NULL;
	self->end = NULL;
	self->exc = NULL;

	#ifdef DEBUG
		PySys_WriteStderr("Scss BlockLocator object rewound!\n");
	#endif
}

static void
scss_BlockLocator_dealloc(scss_BlockLocator *self)
{
	free(self->codestr);
	free(self->_codestr);

	self->ob_type->tp_free((PyObject*)self);

	#ifdef DEBUG
		PySys_WriteStderr("Scss BlockLocator object destroyed!\n");
	#endif
}


/*****/

scss_BlockLocator*
scss_BlockLocator_iter(scss_BlockLocator *self)
{
	Py_INCREF(self);
	return self;
}

PyObject*
scss_BlockLocator_iternext(scss_BlockLocator *self)
{
	scss_Callback *fn;
	char c = 0;
	char *codestr_end = self->codestr + self->codestr_sz;

	memset(&self->block, 0, sizeof(scss_Block));

	while (self->codestr_ptr < codestr_end) {
		c = *(self->codestr_ptr);
		if (!c) {
			self->codestr_ptr++;
			continue;
		}

		repeat:

		fn = scss_function_map[
			(int)c +
			256 * self->instr +
			256 * 256 * (int)(self->par != 0) +
			256 * 256 * 2 * (int)(self->depth > 1 ? 2 : self->depth)
		];

		if (fn != NULL) {
			fn(self);
		}

		self->codestr_ptr++;
		if (self->codestr_ptr > codestr_end) {
			self->codestr_ptr = codestr_end;
		}

		if (self->block.error) {
			#ifdef DEBUG
				if (self->block.error < 0) {
					PySys_WriteStderr("Block found!\n");
				} else {
					PySys_WriteStderr("Exception!\n");
				}
			#endif
			return Py_BuildValue(
				"is#s#",
				self->block.lineno,
				self->block.selprop,
				self->block.selprop_sz,
				self->block.codestr,
				self->block.codestr_sz
			);
		}
	}
	if (self->par > 0) {
		if (self->block.error <= 0) {
			self->block.error = 1;
			self->exc = "Missing closing parenthesis somewhere in block";
			#ifdef DEBUG
				PySys_WriteStderr("%s\n", self->exc);
			#endif
		}
	} else if (self->instr != 0) {
		if (self->block.error <= 0) {
			self->block.error = 2;
			self->exc = "Missing closing string somewhere in block";
			#ifdef DEBUG
				PySys_WriteStderr("%s\n", self->exc);
			#endif
		}
	} else if (self->depth > 0) {
		if (self->block.error <= 0) {
			self->block.error = 3;
			self->exc = "Missing closing string somewhere in block";
			#ifdef DEBUG
				PySys_WriteStderr("%s\n", self->exc);
			#endif
		}
		if (self->init < codestr_end) {
			c = '}';
			goto repeat;
		}
	}
	if (self->init < codestr_end) {
		self->init = codestr_end;
		c = 0;
		goto repeat;
	}

	scss_BlockLocator_rewind(self);

	if (self->exc) {
		PyErr_SetString(PyExc_Exception, self->exc);
		return NULL;
	}

	/* Raising of standard StopIteration exception with empty value. */
	PyErr_SetNone(PyExc_StopIteration);
	return NULL;
}


/* Type definition */

static PyTypeObject scss_BlockLocatorType = {
	PyObject_HEAD_INIT(NULL)
	0,                                         /* ob_size */
	"scss._BlockLocator",                      /* tp_name */
	sizeof(scss_BlockLocator),                 /* tp_basicsize */
	0,                                         /* tp_itemsize */
	(destructor)scss_BlockLocator_dealloc,     /* tp_dealloc */
	0,                                         /* tp_print */
	0,                                         /* tp_getattr */
	0,                                         /* tp_setattr */
	0,                                         /* tp_compare */
	0,                                         /* tp_repr */
	0,                                         /* tp_as_number */
	0,                                         /* tp_as_sequence */
	0,                                         /* tp_as_mapping */
	0,                                         /* tp_hash  */
	0,                                         /* tp_call */
	0,                                         /* tp_str */
	0,                                         /* tp_getattro */
	0,                                         /* tp_setattro */
	0,                                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER, /* tp_flags */
	"Internal BlockLocator iterator object.",  /* tp_doc */
	0,                                         /* tp_traverse */
	0,                                         /* tp_clear */
	0,                                         /* tp_richcompare */
	0,                                         /* tp_weaklistoffset */
	(getiterfunc)scss_BlockLocator_iter,       /* tp_iter: __iter__() method */
	(iternextfunc)scss_BlockLocator_iternext,  /* tp_iternext: next() method */
};


/* Python constructor */

static PyObject *
scss_locate_blocks(PyObject *self, PyObject *args)
{
	PyObject *result = NULL;

	char *codestr;
	int codestr_sz;

	if (PyArg_ParseTuple(args, "s#", &codestr, &codestr_sz)) {
		result = scss_BlockLocator_new(codestr, codestr_sz);
	}

	return result;
}


/* Module functions */

static PyMethodDef scssMethods[] = {
	{"locate_blocks",  scss_locate_blocks, METH_VARARGS, "Locate Scss blocks."},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};


/* Module init function */

PyMODINIT_FUNC
init_scss(void)
{
	PyObject* m;

	scss_BlockLocatorType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&scss_BlockLocatorType) < 0)
		return;

	m = Py_InitModule("_scss", scssMethods);

	init_function_map();

	Py_INCREF(&scss_BlockLocatorType);
	PyModule_AddObject(m, "_BlockLocator", (PyObject *)&scss_BlockLocatorType);
}
