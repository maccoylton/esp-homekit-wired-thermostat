# Component makefile for smtp_impl


INC_DIRS += $(smtp_impl_ROOT)


smtp_impl_INC_DIR = $(smtp_impl_ROOT)
smtp_impl_SRC_DIR = $(smtp_impl_ROOT)

$(eval $(call component_compile_rules,smtp_impl))
