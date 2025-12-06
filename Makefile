# --- 設定區 ---
CXX      := g++
CXXFLAGS := -std=c++17 -DABC_USE_STDINT_H=1
INCLUDES := -Ithird_party/abc/src

ABC_LIB  := third_party/abc/libabc.a
LIBS     := -lm -ldl -lreadline -lpthread -lrt

# --- Python 虛擬環境 ---
VENV     := .venv
PYTHON   := $(VENV)/bin/python

# --- 自動搜尋與排除區 ---

# 1. 搜尋 src/ 下所有子資料夾中的 .cpp 檔案
ALL_CPPS := $(wildcard src/*/*.cpp)

# 2. 產生對應的執行檔列表
#    例如：src/example/main.cpp -> bin/example/main
BINS     := $(patsubst src/%.cpp, bin/%, $(ALL_CPPS))

.PHONY: all clean help venv cirbo

all: $(BINS)

# --- 編譯規則 ---

# 如果 third_party/abc/libabc.a 不存在，就自動進入該資料夾執行 make
$(ABC_LIB):
	@echo "Building ABC library..."
	@cd third_party/abc && $(MAKE) -j8 libabc.a

# 規則：bin/資料夾/檔名 依賴於 src/資料夾/檔名.cpp
# mkdir -p $(dir $@) 會自動建立對應的資料夾 (例如 bin/example/)
bin/%: src/%.cpp $(ABC_LIB)
	@echo "Compiling $@ (source: $<)..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(ABC_LIB) $(LIBS)

clean:
	rm -rf bin abc.history

help:
	@echo "Found sources: $(ALL_CPPS)"
	@echo "Targets:       $(BINS)"

# --- Python 虛擬環境與 cirbo ---
venv:
	python3 -m venv $(VENV)
	. $(VENV)/bin/activate && pip install -U pip && pip install -e third_party/cirbo

cirbo: venv
	. $(VENV)/bin/activate && $(PYTHON) src/cirbo/main.py