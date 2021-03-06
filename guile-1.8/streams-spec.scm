;;; libchop -- a utility library for distributed storage and data backup
;;; Copyright (C) 2008, 2010  Ludovic Courtès <ludo@gnu.org>
;;; Copyright (C) 2005, 2006, 2007  Centre National de la Recherche Scientifique (LAAS-CNRS)
;;;
;;; Libchop is free software: you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation, either version 3 of the License, or
;;; (at your option) any later version.
;;;
;;; Libchop is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with libchop.  If not, see <http://www.gnu.org/licenses/>.

(define-module (streams-spec)
  #:use-module (core-spec)
  #:use-module (filters-spec)

  #:use-module (oop goops)
  #:use-module (g-wrap)
  #:use-module (g-wrap c-codegen)
  #:use-module (g-wrap rti)
  #:use-module (g-wrap c-types)
  #:use-module (g-wrap ws standard)

  ;; Guile-specific things
  #:use-module (g-wrap guile)
  #:use-module (g-wrap guile ws standard)

  #:export (<chop-stream-wrapset>))


;; the wrapset itself.

(define-class <chop-stream-wrapset> (<gw-guile-wrapset>)
  #:id 'streams
  #:dependencies '(standard core filters))


(define-method (global-declarations-cg (ws <chop-stream-wrapset>))
  (list (next-method)
	"#include <chop/chop.h>\n#include <chop/streams.h>\n\n"
	"#include \"core-support.h\"\n"
	"#include \"streams-support.c\"\n\n"))

(define-method (initializations-cg (ws <chop-stream-wrapset>) error-var)
  (list (next-method)
        "chop_scm_init_stream_port_type ();\n"))


(define-method (initialize (ws <chop-stream-wrapset>) initargs)
  (format #t "initializing ~a~%" ws)

  (slot-set! ws 'shlib-path "libguile-chop")

  (next-method ws (append '(#:module (chop streams)) initargs))

  (wrap-constant! ws
		  #:name 'stream/end
		  #:type 'long
		  #:value "CHOP_STREAM_END"
		  #:description "End of stream")

  (wrap-as-chop-object! ws
			#:name '<stream>
			#:c-type-name "chop_stream_t *"
			#:c-const-type-name "const chop_stream_t *")

  (wrap-function! ws
		  #:name 'file-stream-open
		  #:returns '<errcode>
		  #:c-name "chop_file_stream_open_alloc"
		  #:arguments '(((mchars caller-owned) path)
				((<stream> out) stream)))

  (wrap-function! ws
		  #:name 'mem-stream-open
		  #:returns '<stream>
		  #:c-name "chop_mem_stream_open_alloc"
		  #:arguments '((scm vector)))

  (wrap-function! ws
		  #:name 'filtered-stream-open
		  #:returns '<errcode>
		  #:c-name "chop_filtered_stream_open_alloc"
		  #:arguments '(((<stream> aggregated) backend)
				((<filter> aggregated) filter)
				(bool  close-backend? (default #f))
				((<stream> out)        stream)))

  (wrap-function! ws
		  #:name 'stream-read!
		  #:returns '<errcode>
		  #:c-name "chop_stream_read"
		  #:arguments '(((<stream> caller-owned) stream)
				(<writable-input-buffer> buffer)
				((unsigned-int out) read))
		  #:description "Read from @var{stream}.")

  (wrap-function! ws
		  #:name 'stream-close
		  #:returns 'void
		  #:c-name "chop_stream_close"
		  #:arguments '((<stream> stream)))

  (wrap-function! ws
                  #:name 'stream->port
                  #:returns 'scm
                  #:c-name "make_stream_port"
                  #:arguments '((scm  stream))))

;; Local Variables:
;; mode: scheme
;; scheme-program-name: "guile"
;; End: