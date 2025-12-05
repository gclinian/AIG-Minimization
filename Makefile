# --- 設定區 ---
CXX      := g++
CXXFLAGS := -std=c++17 -DABC_USE_STDINT_H=1
INCLUDES := -Ithird_party/abc/src

ABC_LIB  := third_party/abc/libabc.a
LIBS     := -lm -ldl -lreadline -lpthread -lrt

# --- 自動搜尋與排除區 ---

# 1. 搜尋所有第一層子資料夾中的 .cpp 檔案
ALL_CPPS := $(wildcard */*.cpp)

# 2. 排除掉以 third_party/ 開頭的檔案
SRCS     := $(filter-out third_party/%, $(ALL_CPPS))

# 3. 產生對應的執行檔列表，但加上 bin/ 前綴
# 語法：$(patsubst 原模式, 新模式, 列表)
# 例如：example/main.cpp -> bin/example/main
BINS     := $(patsubst %.cpp, bin/%, $(SRCS))

.PHONY: all clean help

all: $(BINS)

# --- 編譯規則 ---

# 如果 third_party/abc/libabc.a 不存在，就自動進入該資料夾執行 make
$(ABC_LIB):
	@echo "Building ABC library..."
	@cd third_party/abc && $(MAKE) -j8 libabc.a

# 規則：bin/資料夾/檔名 依賴於 資料夾/檔名.cpp
# mkdir -p $(dir $@) 會自動建立對應的資料夾 (例如 bin/example/)
bin/%: %.cpp $(ABC_LIB)
	@echo "Compiling $@ (source: $<)..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(ABC_LIB) $(LIBS)

clean:
	rm -rf bin abc.history

help:
	@echo "Found sources: $(SRCS)"
	@echo "Targets:       $(BINS)"