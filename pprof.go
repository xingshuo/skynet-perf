package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"sort"
	"strings"
    "github.com/goccy/go-graphviz"
)

const (
	HEADER_LEN = 4
	KRED = "\x1B[31m"
	KNRM = "\x1B[0m"
)

type Sample struct {
	count int
	depth int
	stack []uint32
}

type Profile struct {
	funcName2Id map[string]uint32
	funcId2Name map[uint32]string
	samples     []*Sample
}

var (
	input string
	text  bool
	png   string
	svg   string
)

func parseFile(filename string) (*Profile, error) {
	data, err := ioutil.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("ReadFile fail: %w", err)
	}
	totalLen := len(data)
	if totalLen < HEADER_LEN {
		return nil, fmt.Errorf("header len error")
	}
	bodyLen := binary.BigEndian.Uint32(data)
	if totalLen != HEADER_LEN+int(bodyLen) {
		return nil, fmt.Errorf("data len error")
	}
	prof := &Profile{
		funcName2Id: make(map[string]uint32),
		funcId2Name: make(map[uint32]string),
		samples:     make([]*Sample, 0),
	}
	offset := HEADER_LEN
	funcNum := binary.BigEndian.Uint32(data[offset:])
	offset += 4
	for i := 0; i < int(funcNum); i++ {
		nameLen := int(data[offset])
		offset += 1
		name := string(data[offset : offset+nameLen])
		offset += nameLen
		Id := binary.BigEndian.Uint32(data[offset:])
		offset += 4
		prof.funcName2Id[name] = Id
		prof.funcId2Name[Id] = name
	}
	for offset < totalLen {
		sa := &Sample{
			count: 0,
			depth: 0,
		}
		sa.count = int(binary.BigEndian.Uint32(data[offset:]))
		offset += 4
		sa.depth = int(binary.BigEndian.Uint32(data[offset:]))
		offset += 4
		sa.stack = make([]uint32, sa.depth)
		for i := 0; i < sa.depth; i++ {
			sa.stack[i] = binary.BigEndian.Uint32(data[offset:])
			offset += 4
		}
		prof.samples = append(prof.samples, sa)
	}
	return prof, nil
}

func showText(prof *Profile) {
	leafNodes := make(map[uint32]int)
	totalCount := 0
	for _, sa := range prof.samples {
		Id := sa.stack[sa.depth-1]
		leafNodes[Id] += sa.count
		totalCount += sa.count
	}
	type pair struct {
		Id    uint32
		count int
	}
	pairs := make([]*pair, 0)
	for Id, count := range leafNodes {
		pairs = append(pairs, &pair{
			Id:    Id,
			count: count,
		})
	}
	sort.Slice(pairs, func(i, j int) bool {
		return pairs[i].count > pairs[j].count
	})

	for i, pa := range pairs {
		s := fmt.Sprintf("%vth\t%v%%\t%v\n", i+1, pa.count*100/totalCount, prof.funcId2Name[pa.Id])
		if i < 5 && pa.count > 0 {
			s = KRED + s + KNRM
		}
		fmt.Print(s)
	}
}

func showPic(prof *Profile, png, svg string) {
	dot := newDot(prof)
	graph, err := graphviz.ParseBytes([]byte(dot))
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
	if png != "" {
		g := graphviz.New()
		err = g.RenderFilename(graph, graphviz.PNG, png)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
	}
	if svg != "" {
		g := graphviz.New()
		err = g.RenderFilename(graph, graphviz.SVG, svg)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
	}
}

func newDot(prof *Profile) string {
	flatNodes := make(map[uint32]int)
	cumNodes := make(map[uint32]int)
	totalCount := 0
	for _, sa := range prof.samples {
		records := make(map[uint32]bool)
		for _, Id := range sa.stack {
			if !records[Id] {
				records[Id] = true
				cumNodes[Id] += sa.count
			}
		}
		leafId := sa.stack[sa.depth-1]
		flatNodes[leafId] += sa.count
		totalCount += sa.count
	}

	type pair struct {
		Id    uint32
		count int
	}
	pairs := make([]*pair, 0)
	for Id, count := range flatNodes {
		pairs = append(pairs, &pair{
			Id:    Id,
			count: count,
		})
	}
	sort.Slice(pairs, func(i, j int) bool {
		return pairs[i].count > pairs[j].count
	})
	ranking := make(map[uint32]int)
	for i, pa := range pairs {
		ranking[pa.Id] = i + 1
	}

	type vector struct {
		src uint32
		dst uint32
	}
	vectors := make(map[vector]int)
	parents := make(map[uint32]bool)
	for _, sa := range prof.samples {
		records := make(map[vector]bool)
		for i := 0; i < sa.depth-1; i++ {
			vec := vector{
				src: sa.stack[i],
				dst: sa.stack[i+1],
			}
			if !records[vec] {
				records[vec] = true
				vectors[vec] += sa.count
			}
			parents[vec.src] = true
		}
	}

	fixFuncName := func(name string) string {
		return strings.Replace(name, "\"", "'", -1)
	}
	dot := fmt.Sprintf("digraph G {\n")
	for Id, count := range cumNodes {
		dot += fmt.Sprintf("\tnode%v [label=\"%v\\r%v (%v%%)\\r",
			Id, fixFuncName(prof.funcId2Name[Id]), flatNodes[Id], flatNodes[Id]*100/totalCount)
		if parents[Id] {
			dot += fmt.Sprintf("%v (%v%%)\\r", count, count*100/totalCount)
		}
		dot += fmt.Sprintf("\";")

		fontsize := flatNodes[Id] * 100 / totalCount
		if fontsize < 10 {
			fontsize = 10
		}
		dot += fmt.Sprintf("fontsize=%v;", fontsize)
		dot += fmt.Sprintf("shape=box;")
		if ranking[Id] > 0 && ranking[Id] <= 5 {
			dot += fmt.Sprintf("color=red;")
		}
		dot += fmt.Sprintf("];\n")
	}

	for vec, count := range vectors {
		linewidth := float64(count) * 8.0 / float64(totalCount)
		if linewidth < 0.2 {
			linewidth = 0.2
		}
		dot += fmt.Sprintf("\tnode%v->node%v [style=\"setlinewidth(%v)\" label=%v];\n", vec.src, vec.dst, linewidth, count)
	}
	dot += fmt.Sprintf("}\n")

	return dot
}

func main() {
	flag.StringVar(&input, "i", "", "input file")
	flag.BoolVar(&text, "text", false, "show text sort data")
	flag.StringVar(&png, "png", "", "generate png file")
	flag.StringVar(&svg, "svg", "", "generate svg file")
	flag.Parse()
	if input == "" {
		flag.Usage()
		os.Exit(1)
	}
	prof, err := parseFile(input)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
	if text {
		showText(prof)
	}
	if png != "" || svg != "" {
		showPic(prof, png, svg)
	}
}
