CC=clang
BUILD_DIR:=build

.PHONY: all
all: main

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

CIMGUI_DIR:=$(CURDIR)/cimgui
CIMGUI_IMGUI_DIR:=$(CIMGUI_DIR)/imgui
CIMGUI_IMGUI_BACKENDS_DIR:=$(CIMGUI_IMGUI_DIR)/backends
CIMGUI_FLAGS:=-O3 -fno-exceptions -fno-rtti -std=c++11 -I$(CIMGUI_IMGUI_DIR) -I$(CIMGUI_IMGUI_DIR)/backends -DIMGUI_IMPL_API="extern \"C\""
CIMGUI_BUILD_DIR:=$(BUILD_DIR)/cimgui
CIMGUI_SOURCES:=$(CIMGUI_DIR)/cimgui.cpp $(CIMGUI_IMGUI_DIR)/imgui.cpp $(CIMGUI_IMGUI_DIR)/imgui_demo.cpp $(CIMGUI_IMGUI_DIR)/imgui_draw.cpp $(CIMGUI_IMGUI_DIR)/imgui_tables.cpp $(CIMGUI_IMGUI_DIR)/imgui_widgets.cpp $(CIMGUI_IMGUI_BACKENDS_DIR)/imgui_impl_glfw.cpp $(CIMGUI_IMGUI_BACKENDS_DIR)/imgui_impl_opengl3.cpp
CIMGUI_OBJS := $(addprefix $(CIMGUI_BUILD_DIR)/, $(addsuffix .o, $(basename $(notdir $(CIMGUI_SOURCES)))))
$(CIMGUI_BUILD_DIR): | $(BUILD_DIR)
	mkdir $@
$(CIMGUI_BUILD_DIR)/%.o: $(CIMGUI_DIR)/%.cpp | $(CIMGUI_BUILD_DIR)
	$(CC) $(CIMGUI_FLAGS) -c -o $@ $<
$(CIMGUI_BUILD_DIR)/%.o: $(CIMGUI_IMGUI_DIR)/%.cpp | $(CIMGUI_BUILD_DIR)
	$(CC) $(CIMGUI_FLAGS) -c -o $@ $<
$(CIMGUI_BUILD_DIR)/imgui_impl_glfw.o: $(CIMGUI_IMGUI_BACKENDS_DIR)/imgui_impl_glfw.cpp | $(CIMGUI_BUILD_DIR)
	$(CC) $(CIMGUI_FLAGS) -c -o $@ $<
$(CIMGUI_BUILD_DIR)/imgui_impl_opengl3.o: $(CIMGUI_IMGUI_BACKENDS_DIR)/imgui_impl_opengl3.cpp | $(CIMGUI_BUILD_DIR)
	$(CC) $(CIMGUI_FLAGS) -c -o $@ $< 
CIMGUI_LIB_STATIC:=$(CIMGUI_BUILD_DIR)/libcimgui.a
$(CIMGUI_LIB_STATIC): $(CIMGUI_OBJS) | $(CIMGUI_BUILD_DIR)
	ar -rc $@ $(CIMGUI_OBJS)

.PHONY: cimgui
cimgui: $(CIMGUI_LIB_STATIC)

$(BUILD_DIR)/glad.o: $(CURDIR)/glad/glad.c | $(BUILD_DIR)
	$(CC) $< -c -o $@ -std=c11 -Weverything -O3 -I $(CURDIR)/glad

$(BUILD_DIR)/glad_egl.o: $(CURDIR)/glad/glad_egl.c | $(BUILD_DIR)
	$(CC) $< -c -o $@ -std=c11 -Weverything -O3 -I $(CURDIR)/glad

.PHONY: main
main: $(BUILD_DIR)/main
$(BUILD_DIR)/main: $(CURDIR)/main.c $(BUILD_DIR)/glad.o $(BUILD_DIR)/glad_egl.o $(CIMGUI_LIB_STATIC) | $(BUILD_DIR)
	$(CC) $(CURDIR)/main.c $(BUILD_DIR)/glad.o $(BUILD_DIR)/glad_egl.o -o $@ -O2 -std=c11 -Werror -Weverything -Wno-variadic-macro-arguments-omitted -Wno-ms-bitfield-padding -Wno-declaration-after-statement -Wno-padded -Wno-unused-variable -Wno-unused-function -Wno-unreachable-code -Wno-unused-macros -Wno-reserved-macro-identifier -Wno-reserved-identifier -lGL -lglfw -lEGL -L$(CIMGUI_BUILD_DIR) -lcimgui -lm -lstdc++ -Wno-address-of-packed-member -Wno-format-nonliteral -Wno-unused-parameter -Wno-unsafe-buffer-usage -Wno-cast-function-type-strict -Wno-pre-c11-compat -Wno-covered-switch-default -lX11 -Wno-unused-but-set-variable

.PHONY: check_egl
check_egl: $(BUILD_DIR)/check_egl
$(BUILD_DIR)/check_egl: $(CURDIR)/check_egl.c | $(BUILD_DIR)
	$(CC) $< -o $@ -std=c99 -lEGL

