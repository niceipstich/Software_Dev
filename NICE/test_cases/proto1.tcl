module mod1 {
    Interface ifce1 {
        port a -direction input -width 4
    }
}

module mod2 {
    Interface ifce2 {
        port b -direction output -width 4
    }
}


module Design {

        connectivity {
            connect ifce1 ifce2 -net
        }

}