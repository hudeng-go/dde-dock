package launcher

import "dde-daemon"

func init() {
	loader.Register(&loader.Module{"launcher", Start, Stop, true})
}
