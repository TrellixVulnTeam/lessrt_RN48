package less.gui.lidar;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

import org.apache.commons.io.FileUtils;
import org.json.JSONObject;

import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.control.Tab;
import javafx.scene.layout.BorderPane;
import javafx.stage.Stage;
import less.LessMainApp;
import less.gui.lidar.model.AlsParameterModel;
import less.gui.lidar.model.BeamParameterModel;
import less.gui.lidar.model.DeviceParameterModel;
import less.gui.lidar.model.MlsParameterModel;
import less.gui.lidar.model.MonoPulseParameterModel;
import less.gui.lidar.model.TlsParameterModel;
import less.gui.lidar.view.AlsParameterController;
import less.gui.lidar.view.BeamParameterController;
import less.gui.lidar.view.DeviceParameterController;
import less.gui.lidar.view.MlsParameterController;
import less.gui.lidar.view.MonoPulseParameterController;
import less.gui.lidar.view.RootLayoutController;
import less.gui.lidar.view.TextParameterViewController;
import less.gui.lidar.view.TlsParameterController;
import less.gui.utils.Const;

public class LiDARMainApp{

	public Stage primaryStage;
	
	public LessMainApp mainApp;
	
	public  BorderPane rootLayout;
	public  RootLayoutController rootLayoutController;
	
	public DeviceParameterModel deviceParameterModel;
	public BeamParameterModel beamParameterModel;
	public MonoPulseParameterModel monoPulseParameterModel;
	public AlsParameterModel alsParameterModel;
	public TlsParameterModel tlsParameterModel;
	public MlsParameterModel mlsParameterModel;
	
	public JSONObject lidarConfigJson;
	
	public void start(Stage primaryStage, LessMainApp mainApp) {
		
		
		this.primaryStage = primaryStage;
		this.mainApp = mainApp;
		this.primaryStage.initOwner(this.mainApp.getPrimaryStage());
		this.primaryStage.setTitle("LiDAR Simulator");
		
		// instantiate models
		instantiate();
		// load fxml of root layout
		initRootLayout();
		
		Scene scene = new Scene(rootLayout);
		primaryStage.setScene(scene);
		primaryStage.show();
		
		// open lidar.conf and set models
		Path path = Paths.get(mainApp.lessMainController.simulation_path, "Parameters", Const.LIDAR_PARAMETER);
		rootLayoutController.open(new File(path.toString()));
		// view data in models
		update();
		// select platform by 'type' in lidar.conf file
		rootLayoutController.open(new File(path.toString()));  // TODO: open twice ?
	}
	
	public void update() {
		
		rootLayoutController.clear();
		
		addDeviceParameter();
		addBeamParameter();
		
		addTextParameterPlatform(new MonoPulseParameterController(monoPulseParameterModel), "Mono Pulse");
		addTextParameterPlatform(new AlsParameterController(alsParameterModel), "ALS");
		addTextParameterPlatform(new TlsParameterController(tlsParameterModel), "TLS");
		addTextParameterPlatform(new MlsParameterController(mlsParameterModel), "MLS");
	}
	
	private void addDeviceParameter() {
		DeviceParameterController deviceParameterController = new DeviceParameterController(deviceParameterModel);
		rootLayoutController.generalParameterVBox.getChildren().add(deviceParameterController.parameterAnchorPane);
	}
	
	private void addBeamParameter() {
		BeamParameterController controller = new BeamParameterController(beamParameterModel);
		rootLayoutController.generalParameterVBox.getChildren().add(controller.parameterAnchorPane);		
	}
	
	private void addTextParameterPlatform(TextParameterViewController controller, String tabText) {
		Tab tab = new Tab();
		tab.setText(tabText);
		tab.setClosable(false);
		tab.setContent(controller.parameterAnchorPane);
		rootLayoutController.platformTabPane.getTabs().add(tab);
	}
	
	private void instantiate() {
		deviceParameterModel = new DeviceParameterModel();		
		beamParameterModel = new BeamParameterModel();
		monoPulseParameterModel = new MonoPulseParameterModel();
		alsParameterModel = new AlsParameterModel();
		tlsParameterModel = new TlsParameterModel();
		mlsParameterModel = new MlsParameterModel();
	}
	
	private void initRootLayout() {
		
		try {
			FXMLLoader loader = new FXMLLoader();
			loader.setLocation(LiDARMainApp.class.getResource("view/RootLayout.fxml"));
			rootLayout = (BorderPane) loader.load();
			
			rootLayoutController = loader.getController();
			rootLayoutController.setMainApp(this);
	
		} catch (Exception e) {
			e.printStackTrace();
		}
		
	}
	
	public Stage getPrimaryStage() {
		return primaryStage;
	}
}
