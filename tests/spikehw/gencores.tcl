
set partname {xc7vx690tffg1761-2}
set ipdir {cores}
set boardname {vc709}

file mkdir $ipdir/$boardname

create_project -name local_synthesized_ip -in_memory -part $partname
set_property board_part xilinx.com:vc709:part0:1.0 [current_project]

proc fpgamake_ipcore {core_name core_version ip_name params} {
    global ipdir boardname

    set generate_ip 0

    if [file exists $ipdir/$boardname/$ip_name/$ip_name.xci] {
    } else {
	puts "no xci file $ip_name.xci"
	set generate_ip 1
    }
    if [file exists $ipdir/$boardname/$ip_name/vivadoversion.txt] {
	gets [open $ipdir/$boardname/$ip_name/vivadoversion.txt r] generated_version
	set current_version [version -short]
	puts "core was generated by vivado $generated_version, currently running vivado $current_version"
	if {$current_version != $generated_version} {
	    puts "vivado version does not match"
	    set generate_ip 1
	}
    } else {
	puts "no vivado version recorded"
	set generate_ip 1
    }

    ## check requested core version and parameters
    if [file exists $ipdir/$boardname/$ip_name/coreversion.txt] {
	gets [open $ipdir/$boardname/$ip_name/coreversion.txt r] generated_version
	set current_version "$core_name $core_version $params"
	puts "Core generated: $generated_version"
	puts "Core requested: $current_version"
	if {$current_version != $generated_version} {
	    puts "core version or params does not match"
	    set generate_ip 1
	}
    } else {
	puts "no core version recorded"
	set generate_ip 1
    }

    if $generate_ip {
	file delete -force $ipdir/$boardname/$ip_name
	file mkdir $ipdir/$boardname
	create_ip -name $core_name -version $core_version -vendor xilinx.com -library ip -module_name $ip_name -dir $ipdir/$boardname
	if [llength $params] {
	    set_property -dict $params [get_ips $ip_name]
	}
        report_property -file $ipdir/$boardname/$ip_name.properties.log [get_ips $ip_name]
	
	generate_target all [get_files $ipdir/$boardname/$ip_name/$ip_name.xci]

	set versionfd [open $ipdir/$boardname/$ip_name/vivadoversion.txt w]
	puts $versionfd [version -short]
	close $versionfd

	set corefd [open $ipdir/$boardname/$ip_name/coreversion.txt w]
	puts $corefd "$core_name $core_version $params"
	close $corefd
    } else {
	read_ip $ipdir/$boardname/$ip_name/$ip_name.xci
    }
    if [file exists $ipdir/$boardname/$ip_name/$ip_name.dcp] {
    } else {
	synth_ip [get_ips $ip_name]
    }
}

fpgamake_ipcore axi_uart16550 2.0  axi_uart16550_1 [list CONFIG.USE_BOARD_FLOW {true} CONFIG.UART_BOARD_INTERFACE {rs232_uart} CONFIG.C_HAS_EXTERNAL_XIN {1} CONFIG.C_HAS_EXTERNAL_RCLK {0} CONFIG.C_EXTERNAL_XIN_CLK_HZ_d {3.686400}  CONFIG.C_EXTERNAL_XIN_CLK_HZ {3686400}]

fpgamake_ipcore axi_intc 4.1 axi_intc_0 [list CONFIG.C_NUM_INTR_INPUTS {16} CONFIG.C_NUM_SW_INTR {0} CONFIG.C_HAS_ILR {1}]
fpgamake_ipcore axi_dma 7.1 axi_dma_0 [list CONFIG.c_sg_include_stscntrl_strm {1} CONFIG.c_m_axi_mm2s_data_width {32} CONFIG.c_m_axi_s2mm_data_width {32} CONFIG.c_mm2s_burst_size {8} CONFIG.c_s2mm_burst_size {8}]

fpgamake_ipcore axi_iic 2.0 axi_iic_0 [list CONFIG.AXI_ACLK_FREQ_MHZ {250} CONFIG.C_GPO_WIDTH {8}]

## does not exist with vivado 2014.2
##fpgamake_ipcore axi_ethernet 7.0 axi_ethernet_0 [list CONFIG.ETHERNET_BOARD_INTERFACE {sfp_sfp1} CONFIG.DIFFCLK_BOARD_INTERFACE {sfp_mgt_clk} CONFIG.axiliteclkrate {250.0} CONFIG.axisclkrate {250.0} CONFIG.PHY_TYPE {1000BaseX}]

## 2014.2: 8.2
## 2015.4: 9.0
fpgamake_ipcore tri_mode_ethernet_mac 9.0 tri_mode_ethernet_mac_0 [list CONFIG.Physical_Interface {Internal}  CONFIG.MAC_Speed {1000_Mbps} CONFIG.SupportLevel {1}]

## 2014.2: 14.2
## 2015.4: 15.1
fpgamake_ipcore gig_ethernet_pcs_pma 15.1 gig_ethernet_pcs_pma_0 [list  CONFIG.USE_BOARD_FLOW {true} CONFIG.Management_Interface {true} CONFIG.ETHERNET_BOARD_INTERFACE {sfp1} CONFIG.DIFFCLK_BOARD_INTERFACE {sfp_mgt_clk} CONFIG.Standard {1000BASEX} CONFIG.SupportLevel {Include_Shared_Logic_in_Core}]

#fpgamake_ipcore axi_gpio 2.0 axi_gpio_0 []

fpgamake_ipcore mig_7series 2.1 ddr3_v2_1 [list CONFIG.XML_INPUT_FILE [pwd]/mig_a.prj CONFIG.RESET_BOARD_INTERFACE {Custom} CONFIG.MIG_DONT_TOUCH_PARAM {Custom} CONFIG.BOARD_MIG_PARAM {Custom}]
